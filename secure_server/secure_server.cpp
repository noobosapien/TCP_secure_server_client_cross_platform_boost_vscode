//////////////////////////////////////////////////////////////
// TCP SECURE SERVER GCC (IPV6 ready)
//
//
// References: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738520(v=vs.85).aspx
//             http://long.ccaba.upc.edu/long/045Guidelines/eva/ipv6.html#daytimeServer6
//
//////////////////////////////////////////////////////////////

#define DEFAULT_PORT "1234"
#define USE_IPV6 true // if set to false, IPv4 addressing scheme will be used; you need to set this to true to
                      // enable IPv6 later on.  The assignment will be marked using IPv6!

#include <deque>
#include <vector>
#include <random>
#include <boost/integer/common_factor_rt.hpp>
#include <boost/math/special_functions/prime.hpp>
#if defined __unix__ || defined __APPLE__
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h> //used by getnameinfo()
#include <iostream>

#include <boost/multiprecision/miller_rabin.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/miller_rabin.hpp>

#include <boost/math/constants/constants.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

using namespace boost::multiprecision;

#elif defined __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h> //required by getaddrinfo() and special constants
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/math/constants/constants.hpp>

using namespace boost::multiprecision;

#define WSVERS MAKEWORD(2, 2) /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
// The high-order byte specifies the minor version number;
// the low-order byte specifies the major version number.
WSADATA wsadata; // Create a WSADATA object called wsadata.
#endif

#define BUFFER_SIZE 500
#define RBUFFER_SIZE 256

namespace bmp = boost::multiprecision;
namespace brand = boost::random;

bmp::cpp_int e, d, rsa_n;
bmp::cpp_int eCA = 29;
bmp::cpp_int dCA{"3109"};
bmp::cpp_int nCA{"3337"};

unsigned int nonce;

/////////////////////////////////////////////////////////////////////
// BOOST

int128_t boost_product(long long A, long long B)
{
   int128_t ans = (int128_t)A * B;

   return ans;
}

// Create a prime number and return it as a cpp_int
// Args:
//    bits: number of bits of the prime number
//    mt19937: mersenne twister generator referene to create random numbers
bmp::cpp_int generate_prime(unsigned int bits, brand::mt19937 &gen)
{
   // gen.seed(time(0)); // Seed the generator with the current time

   bmp::cpp_int value_1 = bmp::pow(bmp::cpp_int(2), bits - 1); // 2^1023 for 1024 bits
   bmp::cpp_int value_2 = bmp::pow(bmp::cpp_int(2), bits) - 1;
   boost::random::uniform_int_distribution<bmp::cpp_int> dist(value_1, value_2); // create a uniform distribution between the 2 values

   bmp::cpp_int result; // The result to send back

   do
   {
      result = dist(gen); // Get a random point in the distribution
   } while (!miller_rabin_test(result, 25, gen) && !miller_rabin_test((result - 1) / 2, 25, gen)); // do the trial 25 times
   // second condition makes sure it is a prime

   return result; // Return the result
}

bmp::cpp_int repeat_square(bmp::cpp_int x, bmp::cpp_int e, bmp::cpp_int n)
{
   bmp::cpp_int y = 1;

   while (e > 0)
   {
      if ((e % 2) == 0)
      {
         x = (x * x) % n;
         e = e / 2;
      }
      else
      {
         y = (x * y) % n;
         e = e - 1;
      }
   }

   return y;
}

bmp::cpp_int simple_extended_euclidean(bmp::cpp_int z, bmp::cpp_int e)
{
   if (bmp::gcd(e, z) != 1)
   {
      return bmp::cpp_int(-1);
   }
   std::deque<bmp::cpp_int> x;
   std::deque<bmp::cpp_int> y;
   std::deque<bmp::cpp_int> w;
   std::deque<bmp::cpp_int> k;

   x.push_back(0);
   x.push_back(1);

   y.push_back(1);
   y.push_back(0);

   w.push_back(e);
   w.push_back(z);

   while (true)
   {
      if (k.size() == 2)
         k.pop_front();
      k.push_back(w[(w.size() - 2)] / w[(w.size() - 1)]);

      if (x.size() == 3)
         x.pop_front();

      x.push_back(x[x.size() - 2] - (k[k.size() - 1] * x[x.size() - 1]));

      if (y.size() == 3)
         y.pop_front();

      y.push_back(y[y.size() - 2] - (k[k.size() - 1] * y[y.size() - 1]));

      if (w.size() == 3)
         w.pop_front();

      w.push_back(w[w.size() - 2] - (k[k.size() - 1] * w[w.size() - 1]));

      if (w[w.size() - 1] == 1)
      {
         break;
      }
   }

   if (y.size())
   {
      if (y[y.size() - 1] < 0)
         return z + y[y.size() - 1];
      return y[y.size() - 1];
   }

   return bmp::cpp_int(0);
}

void create_keys(bmp::cpp_int &e, bmp::cpp_int &d, bmp::cpp_int &n, unsigned int bits)
{
   brand::mt19937 gen;
   gen.seed(32);
   bmp::cpp_int p = generate_prime(bits, gen);
   gen.seed(64);
   bmp::cpp_int q = generate_prime(bits, gen);

   n = p * q;
   bmp::cpp_int z = (p - 1) * (q - 1);

   // get e
   // e = 65537; // Common value for e
   e = generate_prime(bits, gen); // generate a new e

   while ((boost::math::gcd(e, z) != 1 || e == p || e == q) && e < n) // If e and z are not co prime
   {
      e++; // Next number
   }
   // get d
   d = simple_extended_euclidean(z, e);
}

/////////////////////////////////////////////////////////////
// Arbitrary precision data type: We can use any precision with the help of cpp_int data type if we are not sure about
// how much precision is needed in future. It automatically converts the desired precision at run-time.
cpp_int boost_factorial(int num)
{

   cpp_int fact = 1;
   for (int i = num; i > 1; --i)
      fact *= i;
   return fact;
}

////////////////////////////////////////////////////////////
template <typename T>
inline T area_of_a_circle(T r)
{
   // pi represent predefined constant having value
   // 3.1415926535897932384...
   using boost::math::constants::pi;
   return pi<T>() * r * r;
}
/////////////////////////////////////////////////////////////////////

void printBuffer(const char *header, char *buffer)
{
   std::cout << "------" << header << "------" << std::endl;
   for (unsigned int i = 0; i < strlen(buffer); i++)
   {
      if (buffer[i] == '\r')
      {
         std::cout << "buffer[" << i << "]=\\r" << std::endl;
      }
      else if (buffer[i] == '\n')
      {
         std::cout << "buffer[" << i << "]=\\n"
                   << std::endl;
      }
      else
      {
         std::cout << "buffer[" << i << "]=" << buffer[i] << std::endl;
      }
   }
   std::cout << "---" << std::endl;
}

/////////////////////////////////////////////////////////////////////

//*******************************************************************
// MAIN
//*******************************************************************
int main(int argc, char *argv[])
{

   //********************************************************************
   // Boost library test
   //********************************************************************
   // example #1:
   // std::cout << "\n===========================" << std::endl;
   // std::cout << "BOOST BIG NUMBER Example #1: ";
   // std::cout << "\n===========================" << std::endl;

   // long long first = 98745636214564698;
   // long long second = 7459874565236544789;

   // std::cout << "Product of " << first << " * "
   //           << second << " = \n"
   //           << boost_product(first, second) << std::endl;

   create_keys(e, d, rsa_n, 10);

   // bmp::cpp_int cipher = repeat_square(bmp::cpp_int(202301), e, n);
   std::cout << "e: " << e << ", d: " << d << ", n: " << rsa_n << std::endl;
   // std::cout << "cipher: " << cipher << std::endl;
   // std::cout << "message: " << repeat_square(cipher, d, n) << std::endl;

   //-------------------------------------------------

   //********************************************************************
   // INITIALIZATION of the SOCKET library
   //********************************************************************
   // this is a comment
   struct sockaddr_storage clientAddress; // IPV6

   char clientHost[NI_MAXHOST];
   char clientService[NI_MAXSERV];

   char send_buffer[BUFFER_SIZE], receive_buffer[RBUFFER_SIZE];
   int n, bytes, addrlen;
   char portNum[NI_MAXSERV];
   // char username[80];
   // char passwd[80];

   // memset(&localaddr,0,sizeof(localaddr));

#if defined __unix__ || defined __APPLE__
   int s, ns;

#elif defined _WIN32

   SOCKET s, ns;

   //********************************************************************
   // WSSTARTUP
   /*	All processes (applications or DLLs) that call Winsock functions must
      initialize the use of the Windows Sockets DLL before making other Winsock
      functions calls.
      This also makes certain that Winsock is supported on the system.
   */
   //********************************************************************
   int err;

   err = WSAStartup(WSVERS, &wsadata);
   if (err != 0)
   {
      WSACleanup();
      /* Tell the user that we could not find a usable */
      /* Winsock DLL.                                  */
      printf("WSAStartup failed with error: %d\n", err);
      exit(1);
   }

   //********************************************************************
   /* Confirm that the WinSock DLL supports 2.2.        */
   /* Note that if the DLL supports versions greater    */
   /* than 2.2 in addition to 2.2, it will still return */
   /* 2.2 in wVersion since that is the version we      */
   /* requested.                                        */
   //********************************************************************

   if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2)
   {
      /* Tell the user that we could not find a usable */
      /* WinSock DLL.                                  */
      printf("Could not find a usable version of Winsock.dll\n");
      WSACleanup();
      exit(1);
   }
   else
   {
      printf("\n\n<<<TCP SERVER>>>\n");
      printf("\nThe Winsock 2.2 dll was initialised.\n");
   }

#endif

   //********************************************************************
   // set the socket address structure.
   //
   //********************************************************************
   struct addrinfo *result = NULL;
   struct addrinfo hints;
   int iResult;

   //********************************************************************
   // STEP#0 - Specify server address information and socket properties
   //********************************************************************

   // ZeroMemory(&hints, sizeof (hints)); //alternatively, for Windows only
   memset(&hints, 0, sizeof(struct addrinfo));

   if (USE_IPV6)
   {
      hints.ai_family = AF_INET6;
   }
   else
   { // IPV4
      hints.ai_family = AF_INET;
   }

   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_flags = AI_PASSIVE; // For wildcard IP address
                                // setting the AI_PASSIVE flag indicates the caller intends to use
                                // the returned socket address structure in a call to the bind function.

   // Resolve the local address and port to be used by the server
   if (argc == 2)
   {
      iResult = getaddrinfo(NULL, argv[1], &hints, &result); // converts human-readable text strings representing hostnames or IP addresses
                                                             // into a dynamically allocated linked list of struct addrinfo structures
                                                             // IPV4 & IPV6-compliant
      sprintf(portNum, "%s", argv[1]);
      printf("\nargv[1] = %s\n", argv[1]);
   }
   else
   {
      iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result); // converts human-readable text strings representing hostnames or IP addresses
                                                                  // into a dynamically allocated linked list of struct addrinfo structures
                                                                  // IPV4 & IPV6-compliant
      sprintf(portNum, "%s", DEFAULT_PORT);
      printf("\nUsing DEFAULT_PORT = %s\n", portNum);
   }

#if defined __unix__ || defined __APPLE__

   if (iResult != 0)
   {
      printf("getaddrinfo failed: %d\n", iResult);

      return 1;
   }
#elif defined _WIN32

   if (iResult != 0)
   {
      printf("getaddrinfo failed: %d\n", iResult);

      WSACleanup();
      return 1;
   }
#endif

   //********************************************************************
   // STEP#1 - Create welcome SOCKET
   //********************************************************************

#if defined __unix__ || defined __APPLE__
   s = -1;
#elif defined _WIN32
   s = INVALID_SOCKET; // socket for listening
#endif
   // Create a SOCKET for the server to listen for client connections

   s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

#if defined __unix__ || defined __APPLE__

   if (s < 0)
   {
      printf("Error at socket()");
      freeaddrinfo(result);
      exit(1); // return 1;
   }

#elif defined _WIN32

   // check for errors in socket allocation
   if (s == INVALID_SOCKET)
   {
      printf("Error at socket(): %d\n", WSAGetLastError());
      freeaddrinfo(result);
      WSACleanup();
      exit(1); // return 1;
   }
#endif
   //********************************************************************

   //********************************************************************
   // STEP#2 - BIND the welcome socket
   //********************************************************************

   // bind the TCP welcome socket to the local address of the machine and port number
   iResult = bind(s, result->ai_addr, (int)result->ai_addrlen);

#if defined __unix__ || defined __APPLE__
   if (iResult != 0)
   {
      printf("bind failed with error");
      freeaddrinfo(result);

      close(s);

      return 1;
   }

#elif defined _WIN32

   if (iResult == SOCKET_ERROR)
   {
      printf("bind failed with error: %d\n", WSAGetLastError());
      freeaddrinfo(result);

      closesocket(s);
      WSACleanup();
      return 1;
   }
#endif

   freeaddrinfo(result); // free the memory allocated by the getaddrinfo
                         // function for the server's address, as it is
                         // no longer needed
//********************************************************************

/*
   if (bind(s,(struct sockaddr *)(&localaddr),sizeof(localaddr)) == SOCKET_ERROR) {
      printf("Bind failed!\n");
   }
*/

//********************************************************************
// STEP#3 - LISTEN on welcome socket for any incoming connection
//********************************************************************
#if defined __unix__ || defined __APPLE__
   if (listen(s, SOMAXCONN) < 0)
   {
      printf("Listen failed with error\n");
      close(s);

      exit(1);
   }

#elif defined _WIN32
   if (listen(s, SOMAXCONN) == SOCKET_ERROR)
   {
      printf("Listen failed with error: %d\n", WSAGetLastError());
      closesocket(s);
      WSACleanup();
      exit(1);
   }
#endif

   //*******************************************************************
   // INFINITE LOOP
   //********************************************************************
   while (1)
   { // main loop
      printf("\n<<<SERVER>>> is listening at PORT: %s\n", portNum);
      addrlen = sizeof(clientAddress); // IPv4 & IPv6-compliant

//********************************************************************
// NEW SOCKET newsocket = accept
//********************************************************************
#if defined __unix__ || defined __APPLE__
      ns = -1;
#elif defined _WIN32
      ns = INVALID_SOCKET;
#endif

      // Accept a client socket
      // ns = accept(s, NULL, NULL);

      //********************************************************************
      // STEP#4 - Accept a client connection.
      //	accept() blocks the iteration, and causes the program to wait.
      //	Once an incoming client is detected, it returns a new socket ns
      // exclusively for the client.
      // It also extracts the client's IP address and Port number and stores
      // it in a structure.
      //********************************************************************

#if defined __unix__ || defined __APPLE__
      ns = accept(s, (struct sockaddr *)(&clientAddress), (socklen_t *)&addrlen); // IPV4 & IPV6-compliant

      if (ns < 0)
      {
         printf("accept failed\n");
         close(s);

         return 1;
      }
#elif defined _WIN32
      ns = accept(s, (struct sockaddr *)(&clientAddress), &addrlen); // IPV4 & IPV6-compliant
      if (ns == INVALID_SOCKET)
      {
         printf("accept failed: %d\n", WSAGetLastError());
         closesocket(s);
         WSACleanup();
         return 1;
      }
#endif

      printf("\nA <<<CLIENT>>> has been accepted.\n");

      memset(clientHost, 0, sizeof(clientHost));
      memset(clientService, 0, sizeof(clientService));
      getnameinfo((struct sockaddr *)&clientAddress, addrlen,
                  clientHost, sizeof(clientHost),
                  clientService, sizeof(clientService),
                  NI_NUMERICHOST);

      printf("\nConnected to <<<Client>>> with IP address:%s, at Port:%s\n", clientHost, clientService);

      //********************************************************************
      // Communicate with the Client
      //********************************************************************

      // Send the public key
      std::string pub_key = e.str();
      std::string send_n = rsa_n.str();
      std::string send_string = "";

      for (auto c : pub_key)
      {
         bmp::cpp_int cipher = repeat_square(bmp::cpp_int(c), eCA, nCA);
         send_string.append(cipher.str() + ' ');
      }

      send_string.append(": ");

      for (auto c : send_n)
      {
         bmp::cpp_int cipher = repeat_square(bmp::cpp_int(c), eCA, nCA);
         send_string.append(cipher.str() + ' ');
      }
      send_string.push_back('\r');
      send_string.push_back('\n');

      bytes = send(ns, send_string.c_str(), strlen(send_string.c_str()), 0);

      // Wait for ACK
      n = 0;
      while (1)
      {
         bytes = recv(ns, &receive_buffer[n], 1, 0);

         if ((bytes < 0) || (bytes == 0))
            break;

         if (receive_buffer[n] == '\n')
         { /*end on a LF, Note: LF is equal to one character*/
            receive_buffer[n] = '\0';
            break;
         }
         if (receive_buffer[n] != '\r')
            n++; /*ignore CRs*/
      }

      if ((bytes < 0) || (bytes == 0))
         break;

      if (!std::string(receive_buffer).compare("ACK 226 public key recvd"))
      {
         std::cout << "ACK 226 received" << std::endl;
      }
      else
      {
         break;
      }

      // Wait for nonce
      n = 0;
      // Send ACK

      printf("\n--------------------------------------------\n");
      printf("the <<<SERVER>>> is waiting to receive messages.\n");

      while (1)
      {
         n = 0;
         //********************************************************************
         // RECEIVE one command (delimited by \r\n)
         //********************************************************************
         while (1)
         {
            bytes = recv(ns, &receive_buffer[n], 1, 0);

            if ((bytes < 0) || (bytes == 0))
               break;

            if (receive_buffer[n] == '\n')
            { /*end on a LF, Note: LF is equal to one character*/
               receive_buffer[n] = '\0';
               break;
            }
            if (receive_buffer[n] != '\r')
               n++; /*ignore CRs*/
         }

         if ((bytes < 0) || (bytes == 0))
            break;
         sprintf(send_buffer, "Message:'%s' - There are %d bytes of information\r\n", receive_buffer, n);

         //********************************************************************
         // PROCESS REQUEST
         //********************************************************************
         printf("MSG RECEIVED <--: %s\n", receive_buffer);
         // printBuffer("RECEIVE_BUFFER", receive_buffer);

         //********************************************************************
         // SEND
         //********************************************************************
         bytes = send(ns, send_buffer, strlen(send_buffer), 0);
         printf("MSG SENT --> %s\n", send_buffer);
         // printBuffer("SEND_BUFFER", send_buffer);

#if defined __unix__ || defined __APPLE__
         if (bytes < 0)
            break;
#elif defined _WIN32
         if (bytes == SOCKET_ERROR)
            break;
#endif
      }
      //********************************************************************
      // CLOSE SOCKET
      //********************************************************************

#if defined __unix__ || defined __APPLE__
      int iResult = shutdown(ns, SHUT_WR);
      if (iResult < 0)
      {
         printf("shutdown failed with error\n");
         close(ns);

         exit(1);
      }
      close(ns);

#elif defined _WIN32
      int iResult = shutdown(ns, SD_SEND);
      if (iResult == SOCKET_ERROR)
      {
         printf("shutdown failed with error: %d\n", WSAGetLastError());
         closesocket(ns);
         WSACleanup();
         exit(1);
      }

      closesocket(ns);
#endif
      //***********************************************************************

      printf("\ndisconnected from << Client >> with IP address:%s, Port:%s\n", clientHost, clientService);
      printf("=============================================");

   } // main loop
//***********************************************************************
#if defined __unix__ || defined __APPLE__
   close(s);
#elif defined _WIN32
   closesocket(s);
   WSACleanup(); /* call WSACleanup when done using the Winsock dll */
#endif

   return 0;
}
