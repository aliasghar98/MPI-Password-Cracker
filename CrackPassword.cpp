#include <stdio.h>
#include <unistd.h>
#include <mpi.h>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <crypt.h>
#include <fstream>
#include <string>
#include <cstring>
#include <omp.h>
using namespace std;
string getHash(string text);
string getSalt(string text);
string getPassword(long startIdx, long endIdx, string newPassword,string salt, string hashPassword, int &stop_search);
int main(int argc, char *argv[])
{
    int rank,size;
    string text;
    bool hashFound = false;
    bool usernameFound = false;
    int usernameLength = 0;
    char username[100] = {}; 
    char myHash[150] = {};
    char mySalt[50] = {};
    long totalCombos = 217180147158; // 26^8 + 26^7 + 26^6 + 26^5 + 26^4 + ... + 26^1.
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    if (rank == 0) // Master.
    {
        // Reading the username from console.
        cout << "Please enter username: " << endl;
        cin >> username;
        for (int i = 0; username[i] != '\0'; i++)
        {
            usernameLength++;
        }
        
        // Reading shadow file to find username.
        ifstream readFile("shadow");
        while(getline(readFile,text))
        {
            for (int i = 0; text[i] != '\0'; i++)
            {
                if (text[i] != username[i])
                {
                    break;
                }
                else if (i == usernameLength-1)
                {
                    usernameFound = true;
                }
            }
            if (usernameFound == true)
            {
                break;
            }
        }
        readFile.close();
        // Calling getHash function to retrieve the hash.
        string hashPassword = getHash(text);
        // Calling getSalt function to retrieve the salt.
        string salt = getSalt(text);

        // cout << "Hash:" << hashPassword << endl;
        // cout << "Salt:" << salt <<endl;

        
        strcpy(myHash,hashPassword.data());
        strcpy(mySalt,salt.data());
    }
    // Sending hash and salt to all processes.
    MPI_Bcast(myHash,sizeof(myHash),MPI_CHAR,0,MPI_COMM_WORLD);
    MPI_Bcast(mySalt,sizeof(mySalt),MPI_CHAR,0,MPI_COMM_WORLD);
    if (rank == 0) // Master.
    {
        if ( (totalCombos % (size-1)) == 0) // Combinations equally divisible between slaves.
        {
            cout << "Combinations equally divisble between slaves!!!" << endl;
            long slaveCombos = totalCombos / (size-1);
            long startingIdx = 0;
            for (int i = 1; i < size; i++)
            {
                // Sending starting and ending index to each slave to indicate how many combinations they have to cater.
                MPI_Send(&startingIdx,1,MPI_LONG, i, 0, MPI_COMM_WORLD);
                startingIdx = startingIdx + slaveCombos;
                long endingIdx = startingIdx;
                MPI_Send(&endingIdx,1,MPI_LONG,i,0,MPI_COMM_WORLD);
            }

            char pass[10] = {};
            MPI_Recv(pass,10,MPI_CHAR,MPI_ANY_SOURCE,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
            cout << "Password found! Password is : " << pass << endl;
            
            int terminate = 1;
            for (int i = 1; i < size; i++)
            {
                // Sending signal to all processes to terminate.
                MPI_Send(&terminate,1,MPI_INT,i,0,MPI_COMM_WORLD);
            }
        }
        else if ( (totalCombos % (size-1)) != 0) // Not equally divisible
        {
        	long masterCombos = totalCombos % size;
        	long startIdx = totalCombos - masterCombos;

            cout << "Combinations not equally divisble between slaves!!!" << endl;
            long slaveCombos = startIdx / (size-1);
            long startingIdx = 0;
            for (int i = 1; i < size; i++)
            {
                // Sending starting and ending index to each slave to indicate how many combinations they have to cater.
                MPI_Send(&startingIdx,1,MPI_LONG, i, 0, MPI_COMM_WORLD);
                startingIdx = startingIdx + slaveCombos;
                long endingIdx = startingIdx;
                MPI_Send(&endingIdx,1,MPI_LONG,i,0,MPI_COMM_WORLD);
            }
            
            // cout << "Master has : " << masterCombos << " combinations." << endl;
            string generatedPassword = "";
            
            // cout << "StartIdx : " << startIdx << endl;
            string startString = "";
            while (startIdx >= 0)
            {
                // Finding which string should the process start searching from.
                long val1 = startIdx / 26;
                long val2 = startIdx % 26;
                startIdx = val1 - 1;
                startString += char(97 + val2);
            } 
            startString = string(startString.rbegin(),startString.rend());// Reversing string.
            cout << "Master will start searching from string : " << startString << endl;
            int temp = 5;
            // Calling password generator function.
            generatedPassword = getPassword(startIdx,masterCombos+startIdx,startString,mySalt,myHash,temp);
            if (generatedPassword == "") // If password was not found by master.
            {
                // Waiting for slaves to send password after finding it.
                char pass[10] = {};
                MPI_Recv(pass,sizeof(pass),MPI_CHAR,MPI_ANY_SOURCE,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
                // cout << "Password found by Slave! Password is : " << pass << endl;
                
                int terminate = 1;
                for (int i = 1; i < size; i++)
                {
                    // Sending signal to all processes to terminate.
                    MPI_Send(&terminate,1,MPI_INT,i,0,MPI_COMM_WORLD);
                }
            }
            else if (generatedPassword.length() > 1) // If password was found by master.
            {
                cout << "Password found by Master! Password is : " << generatedPassword << endl;
                int terminate = 1;
                for (int i = 1; i < size; i++)
                {
                    // Sending signal to all processes to terminate.
                    MPI_Send(&terminate,1,MPI_INT,i,0,MPI_COMM_WORLD);
                }
            }
        }
    }
    if (rank != 0) // Slaves.
    {   
        string generatedPassword = "";
        long startIdx = 0;
        long endIdx = 0;
        // Receiving both starting and ending index from master.
        MPI_Recv(&startIdx,1,MPI_LONG,0,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
        // cout << "Slave : " << rank << " Received Starting Idx : " << startIdx << endl;
        // sleep(1);
        MPI_Recv(&endIdx,1,MPI_LONG,0,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
        // cout << "Slave : " << rank << " Received Ending Idx : " << endIdx << endl;
        string startString = "";
        while (startIdx >= 0)
        {
            // Finding which string should the process start searching from.
            long val1 = startIdx / 26;
            long val2 = startIdx % 26;
            startIdx = val1 - 1;
            startString += char(97 + val2);
        } // "abc"
        startString = string(startString.rbegin(),startString.rend());// Reversing string.
        cout << "Slave : " << rank << " will start searching from string : " << startString << endl;
        int stop_search = 0;
        #pragma omp parallel num_threads(2) shared(stop_search)
        {
            if (omp_get_thread_num() == 0) // Check if password has been found. If yes, terminate immediately.
            {
                MPI_Recv(&stop_search,1,MPI_INT,0,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
                cout << "Password has been found. Process " << rank << " terminating!!!" << endl;
            }
            else // Search for password...
            {
                generatedPassword = getPassword(startIdx,endIdx,startString,mySalt,myHash, stop_search);
                if (generatedPassword.length() > 1) // If process finds password... send to Master.
                {
                    cout << "Slave " << rank << " has found password : " << generatedPassword << endl;
                    MPI_Send(generatedPassword.data(),generatedPassword.length(),MPI_CHAR,0,0,MPI_COMM_WORLD);
                }
            }
        }
        
    }
    MPI_Finalize();
    return 0;
}
/*
    This function is given a string, starting and ending combination indexes and the target hash.
    It starts from the starting string and searches the number of combinations between start and end indexes.
    In the case where it finds the password that creates the same hash as provided, it returns the password.
*/
string getPassword(long startIdx, long endIdx, string newPassword,string salt, string hashPassword, int &stop_search)
{
    bool foundPassword = false;
    int passwordLength = 8;
    int places[8] = {};
    string generatedHash = "";
    bool lengthReached = false;
    string alphabets[27] = {};
    char input[8] = {};
    for (int i = 0; newPassword[i] != '\0';i++)
    {
        input[i] = newPassword[i];
    }
    for (int i = 1; i < 27; i++)
    {
        alphabets[i] = char(97+(i-1));
    }
    if (newPassword.length() >= 1)
    {
        for (int i = 0; i < newPassword.length(); i++)
        {
            for (int j = 0; j < 27; j++)
            {
                int length = newPassword.length()-i-1;
                const char * input2 = alphabets[j].c_str();
                if ( input[length] == *input2)
                {
                    places[i] = j;
                }

            }
        }
    }
    for (; startIdx < endIdx; startIdx++)
    {
        if (stop_search == 1)
        {
            break;
        }
        for (int i = 0; i < 8; i++)
        {
            if (places[7] == 27)
            {
                lengthReached = true;
                break;
            }
            else if (places[i] == 27)
            {
                if (i == 0)
                {
                    // cout << "Iteration: " << startIdx << endl;
                    // sleep(3);
                    
                }
                places[i+1]++;
                places[i] = 0;
            }
        }

        newPassword = alphabets[places[7]] + alphabets[places[6]] + alphabets[places[5]] + alphabets[places[4]] + alphabets[places[3]] + alphabets[places[2]] + alphabets[places[1]] + alphabets[places[0]];
        places[0]++;
        // cout << newPassword << endl;
        generatedHash = crypt(newPassword.c_str(),salt.c_str());
        generatedHash = getHash(generatedHash);
        if (generatedHash == hashPassword)
        {
            foundPassword = true;
            break;
        }
    }
    if (foundPassword == true)
    {
        return newPassword;
    }
    else
    {
        newPassword = "";
        return newPassword;
    }
}
/*
    Extracts hash part and returns it.
*/
string getHash(string text)
{
    string hashPassword = "";
    int dollarCount = 0;
    for (int i = 0; text[i] != '\0'; i++)
    {
        if (dollarCount >= 3)
        {
            if (text[i] == ':')
            {
                break;
            }
            else
            {
                hashPassword += text[i];
            }
        }
        else if (text[i] == '$')
        {
            dollarCount++;
        }
    }
    return hashPassword;
}
/*
    Extracts salt part and returns it.
*/
string getSalt(string text)
{
    string salt = "";
    int dollarCount = 0;
    for (int i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '$')
        {
            dollarCount++;
            salt += text[i];
            if (dollarCount == 3)
            {
                break;
            }
        }
        else if (dollarCount >= 1)
        {
            salt += text[i];
        }
    }
    return salt;
}
