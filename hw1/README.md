## Computer Network Assignment 1

b06902048	資工三	李峻宇

####Analysis of UDP packets

Screenshot:

![](/Users/lijunyu/NTU/108-1/CN/hw1/UDP.png)

I used wireshark while I was watching youtube vedio by google chrome. Youtube usually provides lots of vedioes to watch.





















#### Analysis of TCP packets

Screenshot : 

![](/Users/lijunyu/NTU/108-1/CN/hw1/TCP.png)

 The port which the server used for this application is 2769. We can find it in Source IP in the screenshot.

####Compare the header between UDP and TCP

There are source port, destination port, length and check sum in both UDP and TCP. 

But there is something only exixting in TCP, like sequence number,  ACK information, which TCP use to remember packets' order and insure stable connection.

























#### Find out a plaintext assword

Screenshot:

![](/Users/lijunyu/NTU/108-1/CN/hw1/plaintext.png)

I connected to `eney.com` and change the URL from `https:` to `http:` , and then loginned while wireshark openning. We can find our `username` and `password` in this packets, so can someone do, that is why it is unsaft.

 