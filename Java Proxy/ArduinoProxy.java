// Copyright 2011, Doug Merrett - Sales Engineering, Salesforce.com Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// - Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// - Neither the name of the salesforce.com nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import java.io.*;
import java.net.*;
import com.sforce.soap.partner.*;
import com.sforce.soap.partner.sobject.*;
import com.sforce.ws.*;

public class ArduinoProxy
{
	static int    port     = 0;  // Port to listen on for the Server
	static String userName = ""; // Salesforce username
	static String passWord = ""; // Salesforce password and security token (if required)


	// Listen for incoming connections and handle them.
	// Will spawn a new thread for every request.
	//
	// There is no way to stop the server main loop,
	// you need to hit Ctrl-C to stop the process.

	public static void main (String [] args)
	{
		// Error handling on parameters
		Boolean hasError = false;

		// The first arg is the port number to use,
		// second is username and
		// last is password (followed by optional security token)
		if (args.length == 3)
		{
			try
			{
				port = Integer.parseInt (args [0]);

				if (port < 1024 || port > 65535)
				{
					System.err.println ("The port must be an integer between 1024 and 65535.");
					hasError = true;
				}
			}
			catch (NumberFormatException e)
			{
				System.err.println ("The port must be an integer between 1024 and 65535.");
				hasError = true;
			}

			userName = args [1];
			passWord = args [2];

			// Check to see the username and password are correct
			try
			{
			    ConnectorConfig SFDCconfig = new ConnectorConfig ();
				SFDCconfig.setUsername (userName);
				SFDCconfig.setPassword (passWord);
				PartnerConnection testConnect = Connector.newConnection (SFDCconfig);
				testConnect.logout ();
			}
			catch (ConnectionException ce)
			{
				System.err.println ("Login to Salesforce failed - please check your username and password");
				hasError = true;
			}
		}
		else
		{
			hasError = true;
		}

		if (hasError)
		{
			System.err.println ("\nUsage: java ArduinoProxy port user pass");
			System.err.println ("     port - an integer between 1024 and 65535");
			System.err.println ("     user - the Salesforce username (testuser@company.com)");
			System.err.println ("     pass - the Salesforce password (followed by optional security token)");
			System.exit (1);
		}

		System.out.println ("Starting ArdinoProxy on port " + port);
		System.out.println ("Press Ctrl-C to exit...");

		try
		{
			ServerSocket listener = new ServerSocket (port);
			Socket server;

			while (true)
			{
				doProxy connection;

				// Blocks the loop until a connection is requested
				server = listener.accept ();

				doProxy proxy = new doProxy (server, userName, passWord);
				Thread proxyThread = new Thread (proxy);
				proxyThread.start ();
			}
		}
		catch (IOException ioe)
		{
			System.err.println ("IOException on socket listen: " + ioe);
			ioe.printStackTrace ();
		}
	}
}

class doProxy implements Runnable
{
	private Socket proxyServer;
	private String requestStr;
	private String userName;
	private String passWord;

	doProxy (Socket serverSocket, String user, String pass)
	{
		proxyServer = serverSocket;
		userName    = user;
		passWord    = pass;
	}

	public void run ()
	{
		// Salesforce configuration stuff
		PartnerConnection connection;
		ConnectorConfig config = new ConnectorConfig ();
		config.setUsername (userName);
		config.setPassword (passWord);

		try
		{
			// Get input and set output to/from the Arduino (client)
			BufferedReader in  = new BufferedReader (new InputStreamReader (proxyServer.getInputStream ()));
			PrintStream    out = new PrintStream (proxyServer.getOutputStream ());

			// create a Salesforce connection object with the credentials
			try
			{
				connection = Connector.newConnection (config);
			}
			catch (ConnectionException ce)
			{
				System.err.println ("Error logging into Salesforce:" + ce);
				ce.printStackTrace ();
				return;
			}

			// Read the request and call Salesforce appropriately
			while ((requestStr = in.readLine()) != null)
			{
				// Parse the String to get the type, name and value
				// the input is either:
				//
				// P,PrinterName,X   X is an integer, 0 = paper out, 1 = OK
				// S,StoreName,X     X is the number of reams of paper

				System.out.println ("Received :" + requestStr + ": from Arduino");
				String [] fields = requestStr.split (",");

				// Is the string valid?
				Boolean OK = (
					          fields.length == 3
							  &&
							  (fields [0].equals ("P") || fields [0].equals ("S"))
							  &&
							  fields [1] != ""
							 );

				if (OK)
				{
					try
					{
						Integer X = Integer.valueOf (fields [2]);
					}
					catch (NumberFormatException nfe)
					{
						OK = false;
						System.out.println ("Invalid string - needs to be 'P/S','Name','Int'");
						out.println ("Error");  // Send Error to Arduino
					}

				}

				if (OK)
				{
					// Query the database to get the Printer/Store, then update it
					Boolean isPrinter = (fields [0].equals ("P"));
					SObject[] recordToUpdate = new SObject [1];

					// Build the query
					String Query = "select Id, Name from " + (isPrinter ? "Printer__c" : "Store__c") + " where Name = '" + fields [1] + "'";
					String Field;
					String Value;

					// Run the query to get the record, then update and save it
					try
					{
						QueryResult queryResults = connection.query (Query);

						if (queryResults.getSize () == 1)
						{
							SObject so = (SObject) queryResults.getRecords ()[0];
							System.out.println ("Updating " + (isPrinter ? "Printer__c" : "Store__c") + " - Id: " + so.getId () + " - Name: " + so.getField ("Name"));

							// Create an SObject to update and only send fields to be updated
							SObject soUpdate = new SObject();
							soUpdate.setType (isPrinter ? "Printer__c" : "Store__c");
							soUpdate.setId (so.getId ());

							if (isPrinter)
							{
								Field = "Offline_Reason__c";
								Value = ((fields [2].equals ("0")) ? "Paper Out" : "Paper OK");
							}
							else
							{
								Field = "Current_Stock__c";
								Value = fields [2];
							}

							// Set the field
							System.out.println ("  Setting " + Field + " to " + Value);
							soUpdate.setField (Field, Value);

							// Save the SObject into the list and then update the sucker...
							recordToUpdate [0] = soUpdate;
							SaveResult [] saveResults = connection.update (recordToUpdate);

							// Check for errors
							if (saveResults [0].isSuccess ())
							{
								System.out.println ("  Successfully updated record - Id: " + saveResults [0].getId ());
								out.println ("OK"); // Send OK to Arduino
							}
							else
							{
								com.sforce.soap.partner.Error [] errors = saveResults [0].getErrors ();
								for (int err = 0; err < errors.length; err++)
								{
									System.out.println ("  Error updating record - Id: " + saveResults [0].getId () + " - " + errors [err].getMessage ());
								}
								out.println ("Error"); // Send Error to Arduino
							}

						}
						else
						{
							System.out.println ("  Error: There were " + queryResults.getSize () + " results from the query and there should be 1.");
							out.println ("Error");  // Send Error to Arduino
						}

					}
					catch (ConnectionException ce)
					{
						System.out.println ("Error calling Salesforce:" + ce);
						ce.printStackTrace ();
						out.println ("Error");  // Send Error to Arduino
					}
				}
				else
				{
					System.out.println ("Invalid string - needs to be 'P/S','Name','Int'");
					out.println ("Error");  // Send Error to Arduino
				}
			}

			// Client shut down the connection, so we shutdown and close
			try
			{
				connection.logout ();
			}
			catch (ConnectionException ce)
			{
				System.err.println ("Error logging out of Salesforce:" + ce);
				ce.printStackTrace ();
			}

			proxyServer.close ();
		}
		catch (IOException ioe)
		{
			System.err.println ("IOException on socket listen: " + ioe);
			ioe.printStackTrace ();
		}
	}
}