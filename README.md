# justin_mstp
The aim of this project is to provide a python access to BACnet-MSTP. To acheive this we used the [Bacnet Stack libary in C] (https://github.com/bacnet-stack/bacnet-stack) to develop a C application. A simple python wrapper is used to start this C application and communicates with it via standard input/output.

## Installation
Follow the following stops to install and use this libary on the raspberry pi or any linux-based computer.

- Clone the github repo 
``` git clone https://github.com/uched41/justin_mstp.git```

- Build the C Application
```
cd ./bacnet_stack/  
make BACDL_DEFINE=-DBACDL_MSTP=1 clean all
``` 

## Usage
The BACnet Server (C application) needs given a port name to be used for MSTP commuinication. This is done by setting the global variable BACNET_MSTP_SERIAL_PORT.
`export BACNET_MSTP_SERIAL_PORT="/dev/ttyUSB0"`. If this is not specified a default port (ttyUSB0) is used. [Example of USB to Serial Converter] (https://www.bb-elec.com/USOPTL4).

### Python Wrapper
The python wrapper provides the interface for communicating with the BACnet Server. It provides the following functions:
- **start_bacnet( )**  
  *params: None*  
  *return: True on Success else False*  
  *function: Starts BACnet server.*  
  
- **stop_bacnet( )**  
  *params: None*  
  *return: True on Success else False*  
  *function: Stop BACnet server.*  

- **update_value( sensor, value )**   
  *params: sensor - `["Temperature", "Humidity", "Pressure", "Flow", "Analog", "Particles"]`*   
  *return: True on Success else False*   
  *function: updates the value of a BACnet object*   
  *example: `update_value("Temperature", 34)`*   
  
  
