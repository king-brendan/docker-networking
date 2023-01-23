Brendan King

---- RUNNING INSTRUCTIONS ----
Build the docker container using the provided Dockerfile, then run using:

    docker build . -t prj1
    docker run --name <container-name> --network <network> --hostname <hostname> prj1 -h hostfile.txt


The network I recommend using is a simple bridge network, created using:

    docker network create --driver bridge mynetwork


The hostfile I provided specifies 5 hosts, named container1, container2, ... container5.
Run the program by creating those separate containers using the command above, where <container-name> == <hostname>.

To view logs and general debugging information, use the -it flag to see messages printed to stdout in real-time.


---- GENERAL INFORMATION ----
This code is non-terminating, meaning that it will continue executing (and sending out UDP messages) until 
terminated by the user. When run without the -it flag, the only visible output will be 'READY' printed to 
stderr upon completion.
The default port number is 3000, but can be configured in the global parameters section.
