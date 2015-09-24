Janus Arduino Library - Software
===

www.janus-rc.com

Janus Arduino software repository. Contains backend library and demo code.

This library should aid in getting a proof of concept with the Janus Plug in Modems done extremely 
quickly, allowing application development to focus on the bigger picture.

These demos are for evaluations purposes only. Janus provides them as-is without any warranty or guarantees. 

### Janus Plugin Library
Arduino Shield library for use with the Janus Plugin Modem or Telit Modem.

* `Baudrate_Change`: Adjust the modem's baud rate.
* `Modem_Information`: Prints out the modem's information, including SIM serial and Network.
* `Modem_Terminal`: Cross connect the UART for quick evaluation and direct AT commands.
* `Modem_USBControl`: Turn the external USB on or off
* `SMS_Echo`: M2M Demo - Listens for incoming SMS, prints the information, and echoes back to the sender.
* `Socket_Dial`: M2M Demo - Dials out to a remote host, opens a serial bridge
* `Socket_Listen`: M2M Demo - Creates a listening host, once a client connects a serial bridge is open.

The Listener demo will create a completely open listener if you have an externally routable IP. Please be careful to shut this down
once testing is complete as you may find someone will connect to it and rack up your data usage. This is by default, but can be changed
in the demo by the user if needed with setupFirewall.





