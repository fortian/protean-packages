This sample http "client-server" application is described in the 
rapr tutorial in the docs directory.

1. Modify the dictionary-http.xml file to reflect the correct
IP addres for the node you wish to use as the http server.

2. To run the application, first start the "http server" on the
server node:

./rapr input http-server.input

On the client node start the client:

./rapr input http-client.input.

(You may modify the test stop times and the periodicity 
interval to shorten or lengthen the test.  See the 
examples in the input scripts.)

3. If you have chosen to log the mgen output, you may use NRL's 
TRPR application to create a gnuplot with the following command:

./trpr drec input mgen-http.log auto X output mgen-http.plot

Display the plot file via:

gnuplot -persist mgen-http.plot

Sample log file data and plotting output from the sample configuration
files are included in this directory.  (The 30 minute test example
was used.)