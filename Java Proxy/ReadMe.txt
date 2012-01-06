The ArduinoProxy class relies upon the SFDC-WSC library.

To build the ArduinoProxy class follow these steps:

  - Download your partner.wsdl from your Salesforce instance (Setup/Develop/API)
  - Download the wsc-XX.jar from http://code.google.com/p/sfdc-wsc/ [The code used wsc-22.jar]
  - Make sure you are using Java 1.6 and run the following command to generate the required partner.jar
	java -classpath wsc-22.jar com.sforce.ws.tools.wsdlc partner.wsdl partner.jar
  - Compile the class with the following command
	javac -cp .\partner.jar;.\wsc-22.jar;. ArduinoProxy.java

To run the ArduinoProxy use this command (using java 1.6)
  - where _PORT_ is the port to listen on for Arduino calls, _USERNAME_ is the salesforce.com username
    of the Arduino and _PASSWORD_ is the password for this user

	java -cp .\partner.jar;.\wsc-22.jar;. ArduinoProxy _PORT_ _USERNAME_ _PASSWORD_
