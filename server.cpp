/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <iostream>
#include <stdio.h>
#include <sys/types.h>   
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h> 
#include <signal.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <string>
#include <sstream>

using namespace std;

string printRequestToConsole(int);
string buildResponseHeader(int, string, int);
void serveFileToClient(string, int);
void sendError(int, string);

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

//code in the main function is from the sample skeleton code provided for CS118
int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     cout << "Listening on port " << argv[1] << endl << endl;
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             //get and print client request message to console and parse request for filename requested 
             string filename = printRequestToConsole(newsockfd);
             //send the requested file to the client
             serveFileToClient(filename, newsockfd);
             close(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

string printRequestToConsole (int sock) {
  int n;
  char buffer[512];
      
  bzero(buffer,512);
  //get the http request 
  n = read(sock,buffer,511);
  if (n < 0) error("ERROR reading from socket");

  cout << "HTTP REQUEST:\n" << buffer << endl;

  //parse the request for the filename by retrieving the string following GET /
  int i;
  for (i = 0; i < 510; i++)
    if (buffer[i] == 'G' && buffer[1+i] == 'E' && buffer[2+i] == 'T')
      break;

  //when GET is matched, i is at char index of G. Increment to position following GET / (5 chars away)
  i+=5;
  
  //store characters following GET / until the next space - that is the requested filename
  string filename;
  while (buffer[i] != ' ') {
    filename += buffer[i];
    i++;
  }

  return filename;
}

void serveFileToClient (string filename, int sock) {
  
  //checks for the case where the client submits a request without a filename
  if (filename == "") {
    sendError(sock, "ERROR no filename provided");
    return;
  }

  //input stream class used to operate on the file requested by the user
  ifstream istr (filename.c_str(), ifstream::in);

  if (istr) {
    //determine the length of the file
    istr.seekg (0, istr.end);
    int fileLength = istr.tellg();
    istr.seekg (0, istr.beg);

    //read in the file
    char *buf = new char [fileLength];
    istr.read(buf, fileLength);

    //generates the response string 
    string httpHeader = buildResponseHeader(sock, filename, fileLength);

    //send the header to the socket
    write(sock, httpHeader.c_str(), httpHeader.length());
    cout << "HTTP RESPONSE:\r\n"<< httpHeader << endl; 

    //send the file to the socket
    write(sock, buf, fileLength);

    cout << "File " << filename << " was served to the client." << endl << endl;
    delete[] buf;
  }
  else {
      //call sendError function to format and send 404 file not found response error
      sendError(sock, "ERROR file was not found");
      return;
  }

}

string buildResponseHeader(int sock, string filename, int filesize) {
  string filetype;
  bool foundExtension = false;

  //determines the length of the filename
  int i;
  for (i = filename.length(); i >= 0; i--) 
    if (filename[i] == '.') { //check for file extensions in filename
      foundExtension = true;
      break;
    }

  //if the file has a file extention like txt or html etc., retrieve it
  if (foundExtension)
    for (i++; i < filename.length(); i++)
      filetype += filename[i];

  string okayStatus200 = "HTTP/1.1 200 OK\r\n";
  string connection = "Connection: close\r\n";

  //get the current time
  time_t rawTimeNow;
  struct tm * curTime;
  char timebuf[80];
  time (&rawTimeNow);
  curTime = gmtime(&rawTimeNow);
  strftime(timebuf, 35, "%a, %d %b %Y %T %Z", curTime);
  string currentTime = timebuf;
  currentTime = "Date: " + currentTime + "\r\n";

  //set the name of the server
  string serverName = "Server: CS118Project1\r\n";

  //get the last modified date and time of the file being sent
  struct tm * timeLastMod;
  struct stat attrib;  
  stat(filename.c_str(), &attrib);     
  timeLastMod = gmtime(&(attrib.st_mtime));
  char timebufLastMod[80];
  strftime(timebufLastMod, 35, "%a, %d %b %Y %T %Z", timeLastMod);
  string lastModifiedTime = timebufLastMod;
  lastModifiedTime = "Last-Modified: " + lastModifiedTime + "\r\n";

  //select the content type based on the file extension
  string contentType = "Content-Type: ";
  if (filetype == "html")
    contentType += "text/html\r\n";
  else if (filetype == "jpeg")
    contentType += "image/jpeg\r\n";
  else if (filetype == "gif")
    contentType += "image/gif\r\n";
  else if (filetype == "txt")
    contentType += "text/plain\r\n";
  else if (filetype == "jpg")
    contentType += "text/jpg\r\n";
  else
    contentType += "unknown\r\n";

  ostringstream responseHeader;

  //format the response header as a concatenation of the above attributes
  responseHeader << okayStatus200 << connection << currentTime << serverName << lastModifiedTime << "Content-Length: " << filesize << "\r\n" << contentType << "\r\n";

  string responseStr = responseHeader.str();

  return responseStr;
}

void sendError (int sock, string errorMessage) {
  string error404BNotFound = "HTTP/1.1 404 Not Found\r\n\r\n";
  string error404NotFoundStr = "Error 404: File Not Found!";

  //send error to socket
  write(sock, error404BNotFound.c_str(), error404BNotFound.length());
  //send error message
  write(sock, error404NotFoundStr.c_str(), error404NotFoundStr.length());
  cout << errorMessage << endl << endl;
  return;
}