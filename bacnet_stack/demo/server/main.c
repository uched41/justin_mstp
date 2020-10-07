#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "server.h"
#include "address.h"
#include "bacdef.h"
#include "handlers.h"
#include "client.h"
#include "dlenv.h"
#include "bacdcode.h"
#include "npdu.h"
#include "apdu.h"
#include "iam.h"
#include "tsm.h"
#include "device.h"
#include "bacfile.h"
#include "datalink.h"
#include "dcc.h"
#include "getevent.h"
#include "net.h"
#include "txbuf.h"
#include "lc.h"
#include "version.h"
/* include the device object */
#include "device.h"
#include "trendlog.h"
#if defined(INTRINSIC_REPORTING)
#include "nc.h"
#endif /* defined(INTRINSIC_REPORTING) */

/* include the device object */
#include "device.h"
#include "ai.h"


/* My objects */
static object_functions_t My_Object_Table[] = {
    {OBJECT_DEVICE,
            NULL /* Init - don't init Device or it will recourse! */ ,
            Device_Count,
            Device_Index_To_Instance,
            Device_Valid_Object_Instance_Number,
            Device_Object_Name,
            Device_Read_Property_Local,
            Device_Write_Property_Local,
            Device_Property_Lists,
            DeviceGetRRInfo,
            NULL /* Iterator */ ,
            NULL /* Value_Lists */ ,
            NULL /* COV */ ,
            NULL /* COV Clear */ ,
        NULL /* Intrinsic Reporting */ },
    {OBJECT_ANALOG_INPUT,
            Analog_Input_Init,
            Analog_Input_Count,
            Analog_Input_Index_To_Instance,
            Analog_Input_Valid_Instance,
            Analog_Input_Object_Name,
            Analog_Input_Read_Property,
            Analog_Input_Write_Property,
            Analog_Input_Property_Lists,
            NULL /* ReadRangeInfo */ ,
            NULL /* Iterator */ ,
            NULL /* Value_Lists */ ,
            NULL /* COV */ ,
            NULL /* COV Clear */ ,
        Analog_Input_Intrinsic_Reporting},

    {MAX_BACNET_OBJECT_TYPE,
            NULL /* Init */ ,
            NULL /* Count */ ,
            NULL /* Index_To_Instance */ ,
            NULL /* Valid_Instance */ ,
            NULL /* Object_Name */ ,
            NULL /* Read_Property */ ,
            NULL /* Write_Property */ ,
            NULL /* Property_Lists */ ,
            NULL /* ReadRangeInfo */ ,
            NULL /* Iterator */ ,
            NULL /* Value_Lists */ ,
            NULL /* COV */ ,
            NULL /* COV Clear */ ,
        NULL /* Intrinsic Reporting */ }
};

/** @file server/main.c  Example server application using the BACnet Stack. */

/** Buffer used for receiving */
static uint8_t Rx_Buf[MAX_MPDU] = { 0 };

/** Initialize the handlers we will utilize.
 * @see Device_Init, apdu_set_unconfirmed_handler, apdu_set_confirmed_handler
 */
static void Init_Service_Handlers(
    void)
{
    Device_Init(My_Object_Table);
    /* we need to handle who-is to support dynamic device binding */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
    /* handle i-am to support binding to other devices */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_bind);
    /* set the handler for all the services we don't implement */
    /* It is required to send the proper reject message... */
    apdu_set_unrecognized_service_handler_handler
        (handler_unrecognized_service);
    /* Set the handlers for any confirmed services that we support. */
    /* We must implement read property - it's required! */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY,
        handler_read_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
        handler_read_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROPERTY,
        handler_write_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE,
        handler_write_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_RANGE,
        handler_read_range);
#if defined(BACFILE)
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_ATOMIC_READ_FILE,
        handler_atomic_read_file);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_ATOMIC_WRITE_FILE,
        handler_atomic_write_file);
#endif
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_REINITIALIZE_DEVICE,
        handler_reinitialize_device);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_UTC_TIME_SYNCHRONIZATION,
        handler_timesync_utc);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_TIME_SYNCHRONIZATION,
        handler_timesync);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV,
        handler_cov_subscribe);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_COV_NOTIFICATION,
        handler_ucov_notification);
    /* handle communication so we can shutup when asked */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
        handler_device_communication_control);
    /* handle the data coming back from private requests */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_PRIVATE_TRANSFER,
        handler_unconfirmed_private_transfer);
#if defined(INTRINSIC_REPORTING)
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_ACKNOWLEDGE_ALARM,
        handler_alarm_ack);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_GET_EVENT_INFORMATION,
        handler_get_event_information);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_GET_ALARM_SUMMARY,
        handler_get_alarm_summary);
#endif /* defined(INTRINSIC_REPORTING) */
}

/** Main function of server demo.
 *
 * @see Device_Set_Object_Instance_Number, dlenv_init, Send_I_Am,
 *      datalink_receive, npdu_handler,
 *      dcc_timer_seconds, bvlc_maintenance_timer,
 *      Load_Control_State_Machine_Handler, handler_cov_task,
 *      tsm_timer_milliseconds
 *
 * @param argc [in] Arg count.
 * @param argv [in] Takes one argument: the Device Instance #.
 * @return 0 on success.
 */
#define TEMP_DATA       0
#define HUMIDITY_DATA   1
#define PRESSURE_DATA   2
#define FLOW_DATA       3
#define ANALOG_DATA     4
#define PARTICLES_DATA  5

int main(
    int argc,
    char *argv[])
{
    BACNET_ADDRESS src = {
        0
    };  /* address where message came from */
    uint16_t pdu_len = 0;
    unsigned timeout = 1;       /* milliseconds */
    time_t last_seconds = 0;
    time_t current_seconds = 0;
    uint32_t elapsed_seconds = 0;
    uint32_t elapsed_milliseconds = 0;
    uint32_t address_binding_tmr = 0;
    uint32_t recipient_scan_tmr = 0;

    /* allow the device ID to be set */
    if (argc > 1)
        Device_Set_Object_Instance_Number(strtol(argv[1], NULL, 0));
    printf("BACnet Server Demo\n" "BACnet Stack Version %s\n"
        "BACnet Device ID: %u\n" "Max APDU: %d\n", BACnet_Version,
        Device_Object_Instance_Number(), MAX_APDU);

    /* Initialize analog device parameters */
    BACNET_CHARACTER_STRING* tstring = malloc(sizeof(BACNET_CHARACTER_STRING)) ;
    characterstring_init_ansi(tstring, "Temperature");
    Analog_Input_Object_Name(TEMP_DATA, tstring);
    Analog_Input_Present_Value_Set(TEMP_DATA, 4);

    characterstring_init_ansi(tstring, "Humidity");
    Analog_Input_Object_Name(HUMIDITY_DATA, tstring);

    characterstring_init_ansi(tstring, "Pressure");
    Analog_Input_Object_Name(PRESSURE_DATA, tstring);

    characterstring_init_ansi(tstring, "Flow");
    Analog_Input_Object_Name(FLOW_DATA, tstring);

    characterstring_init_ansi(tstring, "Analog");
    Analog_Input_Object_Name(ANALOG_DATA, tstring);

    characterstring_init_ansi(tstring, "Particles");
    Analog_Input_Object_Name(PARTICLES_DATA, tstring);

    /* load any static address bindings to show up
       in our device bindings list */
    address_init();
    Init_Service_Handlers();
    dlenv_init();
    atexit(datalink_cleanup);
    /* configure the timeout values */
    last_seconds = time(NULL);
    /* broadcast an I-Am on startup */
    Send_I_Am(&Handler_Transmit_Buffer[0]);

    fd_set readfds;
    FD_ZERO(&readfds);

    struct timeval xtimeout;
    xtimeout.tv_sec = 0;
    xtimeout.tv_usec = 0;

    char message[50];
    int ind = 0; float val = 0;

    /* loop forever */
    for (;;) {
        /* input */
        current_seconds = time(NULL);

        /* returns 0 bytes on timeout */
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);

        /* process */
        if (pdu_len) {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
        }
        /* at least one second has passed */
        elapsed_seconds = (uint32_t) (current_seconds - last_seconds);
        if (elapsed_seconds) {
            last_seconds = current_seconds;
            dcc_timer_seconds(elapsed_seconds);
#if defined(BACDL_BIP) && BBMD_ENABLED
            bvlc_maintenance_timer(elapsed_seconds);
#endif
            dlenv_maintenance_timer(elapsed_seconds);
            Load_Control_State_Machine_Handler();
            elapsed_milliseconds = elapsed_seconds * 1000;
            handler_cov_timer_seconds(elapsed_seconds);
            tsm_timer_milliseconds(elapsed_milliseconds);
            trend_log_timer(elapsed_seconds);
#if defined(INTRINSIC_REPORTING)
            Device_local_reporting();
#endif
        }
        handler_cov_task();
        /* scan cache address */
        address_binding_tmr += elapsed_seconds;
        if (address_binding_tmr >= 60) {
            address_cache_timer(address_binding_tmr);
            address_binding_tmr = 0;
        }
#if defined(INTRINSIC_REPORTING)
        /* try to find addresses of recipients */
        recipient_scan_tmr += elapsed_seconds;
        if (recipient_scan_tmr >= NC_RESCAN_RECIPIENTS_SECS) {
            Notification_Class_find_recipient();
            recipient_scan_tmr = 0;
        }
#endif
        /* output */

        /* blink LEDs, Turn on or off outputs, etc */
        FD_SET(STDIN_FILENO, &readfds);

        if (select(1, &readfds, NULL, NULL, &xtimeout)){
            scanf("%19s", message); 
            printf("%s\n", message); 
            if(sscanf(message, "%d:%f", &ind, &val) == 2){
                if(ind < 6){
                    Analog_Input_Present_Value_Set(ind, val);
                    printf("%d:%f\n", ind, val);
                }
            } 
        }
        
    }

    return 0;
}

/* @} */

/* End group ServerDemo */
