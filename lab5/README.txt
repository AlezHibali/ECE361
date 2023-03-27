For section 2, we implemented "Inactivity Timer" and "User Registration" in our program.

User registration is implemented:
On the client side, we have a reg_user function that is called when command is "/register id pwd ip port". We will connect to server and send a packet of type 15 REGISTER <password> Register with the server. It then waits for respond ACK/NAK and output NAK message if received. 
On the server side, we have a txt file "userlist_db.txt" storing all registered username and password for our server. Whenever the client asks for registration, if the username does not exist in our database, we succesfully register and load data into .txt file. Otherwise send NAK to client.

Inactivity Timer is implemented:
On the client side, if packet type 18 "TIMEOUT <message> Notification to client of timeout/idle" is received, it cuts off connection/socketfd.
On the server side, we have a function called select to time the recv function. If recv is idle for 60 seconds, it will output ERROR: Timeout - Logging out, and execute the logout process, closing socketfd and exiting thread.
