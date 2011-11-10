#include <iostream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <map>
#include <signal.h>
#define ROOT_DIC "/home/PG/PG/"
#define DEBUG 0
#define PIPEMAX 100000
#define PIPEADD 10000000
using namespace std;
void welcome_msg()
{
	cout << "****************************************" << endl;
	cout << "** Welcome to the information server. **" << endl;
	cout << "****************************************" << endl;
}
string i2s(int q)
{
	ostringstream sout;
	sout << q;
	return sout.str();
}
int s2i(string q)
{
	int r;
	istringstream ssin(q);
	ssin >> r;
	return r;
}
class PG_pipe
{
public:
	struct pipe_unit
	{
		int fd[2];
		int flag;
		pipe_unit(){flag = 0;}
		
	};

	map<int, pipe_unit> p_table;
	map<int, int> relation, fd_table;

	map<int, pipe_unit>::iterator iter,iter2;
	PG_pipe()
	{
		fd_table[0] = -1;
		fd_table[1] = -1;
		fd_table[2] = -1;
	}
	void show()
	{

		map<int,int>::iterator i;
		cerr << "pid " << getpid() << endl;
		for (i = fd_table.begin(); i != fd_table.end(); ++i)
		{
			if (i->second != 0)cerr << i->first << " " << i->second << endl;
		}
	}
	void create(int q)
	{
		if (p_table.find(q) != p_table.end()) return;
		
		pipe(p_table[q].fd);
		fd_table[p_table[q].fd[0]] = p_table[q].fd[0];
		fd_table[p_table[q].fd[1]] = p_table[q].fd[1];

	}
	void close2(int q)
	{
		close(q);
		fd_table[q] = 0;
	}
	void dup22(int a,int b)
	{
		dup2(a,b);
		fd_table[b] = a;
	}
	void clean_pipe()
	{
		map<int,int>::iterator i;
		for (i = fd_table.begin(); i != fd_table.end(); ++i)
		{
			if (i->second > 2)
				close2(i->second);
		}
	}
	void fix_stdin(int q)
	{
		
		iter = p_table.find(q);
		if (iter == p_table.end()) return;

		close2(0);
		dup22(iter->second.fd[0], 0);
		close2(iter->second.fd[0]);

	}
	void fix_stdout(int q,int fix_stderr)
	{
		int aim = relation[q];
		iter = p_table.find(aim);
		if (iter == p_table.end()) return;

		close2(1);
		dup22(iter->second.fd[1], 1);
		if (fix_stderr) dup22(iter->second.fd[1], 2);
		close2(iter->second.fd[1]);
		
	}
	void fix_main(int q)
	{
		iter = p_table.find(q);
		if (iter == p_table.end()) return;

		close2(iter->second.fd[0]);
		close2(iter->second.fd[1]);
	}
	void connect(int a, int b)
	{

		relation[a] = b;
		create(b);
	}
	int chk_connect(int q){return relation[q];}
	void redirect_to_file(string fn)
	{
		int fd = open(fn.c_str(),O_WRONLY | O_CREAT | O_TRUNC,700);
		close2(1);
		dup22(fd,1);
		close2(fd);
	}
	
};
class PG_cmd
{
public:
	string cmd;
	vector<string> list;
	string PATH[101];
	vector<int> pipe_seg,pipe_seg_flag;
	map<string, string> ENV;
	int delay, delay_type;
	int seq_no, PATH_size;
	string prefix;
	string redirect_from, redirect_to; 
	int size;
	bool exit_flag, pipe_err_flag;
	
	PG_cmd()
	{
		size = 0;
		redirect_from = "";
		redirect_to = "";
		prefix = "cmd_";
		exit_flag = 0;
		pipe_err_flag = 0;
		
		PATH[0] = "bin";
		PATH[1] = "bin"; 
		PATH_size = 1;
		ENV["PATH"] = "bin";
	}
	void read()
	{
		getline(cin, cmd);
	}
	void parse()
	{
		string tmp = "";
		redirect_to = "";
		delay = 0;
		list.clear();
		
		/***********************************************************************************************/
		for (int i = 0; i < cmd.size(); i++)
		{
			if (cmd[i] == ' ' || cmd[i] == '\t' || cmd[i] == '\n' || cmd[i] == '\r')
			{
				if (tmp != "")
				{
					list.push_back(tmp);
					tmp = "";
				}
			}
			else tmp += cmd[i];
		}
		if (tmp != "")list.push_back(tmp);
		/***********************************************************************************************/
		
		for (int i = 0; i < list.size(); i++)
		{
			if(list[i] == ">")
			{
				redirect_to = list[i+1];
				list.erase(list.begin() + i); list.erase(list.begin() + i);
			}
		} 
		int end = list.size() - 1;
		if(list[end][0] =='|' || list[end][0] == '!')
		{
			if (list[end][0] == '!') pipe_err_flag = 1;
			list[end][0]=' ';
			delay = s2i(list[end]);
			list.erase(list.begin() + end);
		}
		pipe_seg.clear();
		pipe_seg_flag.clear();
		pipe_seg.push_back(-1);
		for (int i = 1; i < list.size(); i++)
		{	
			if (list[i] == "|" || list[i] == "!")
			{
				pipe_seg.push_back(i);
				pipe_seg_flag.push_back(list[i] == "!" ? 1:0);
			}			
		}
		pipe_seg.push_back(list.size());
		
		/***********************************************************************************************/
		if (list[0] == "exit")
		{
			exit_flag = 1;
		}
	}
	void show()
	{
		cerr << "size " << list.size() << endl;
		for (int i = 0; i < list.size(); i++)
		cout << list[i] << endl;
	}
	void exec(int from, int to)
	{
		if (list[from] == "setenv")
		{
			ENV[list[from+1]] = list[from+2];
			if (list[from+1] != "PATH")return;
			PATH[0] = list[from+2];
			PATH_size = 0;
			for (int i = 0; i < PATH[0].size(); i++)
			{
				PATH[++PATH_size] = "";
				while (i < PATH[0].size() && PATH[0][i] !=':')
				{
					PATH[PATH_size] += PATH[0][i];
					i++;
				} 
			}
			exit(0);
		}
		if (list[from] == "printenv")
		{
			cout << ENV[list[from+1]] << endl;
			exit(0);
		}
		struct stat statbuf;
		bool success_flag = 0;
		string aim;
		for (int i=1; i<=PATH_size; i++)
		{
			
			aim =ROOT_DIC + PATH[i] + "/" + list[from];

			if (stat(aim.c_str(), &statbuf) == -1) continue;
			else
			{
				success_flag = 1;
				break;
			}
		}
		if (!success_flag)
		{
			cerr << "Unknown command: [" << list[from] << "]." << endl;
			exit(0);
		}
		
		/***********************************************************************************************/
		
		char **tmp = new char*[1001];
		int i, cnt;
		if(from > to)return;

		for (i = from,cnt = 0; i <= to; i++,cnt++)
		{

			tmp[cnt] = new char[1001];
			strcpy(tmp[cnt], list[i].c_str());
		}
		tmp[cnt] = NULL;

		if (execv(aim.c_str(), tmp))
		{
			perror("excvp fail");
			exit(0);
		}
	}
	void exec() {exec(0,list.size()-1);}
	void exec_seg(int q){exec(pipe_seg[q]+1, pipe_seg[q+1]-1);}

};
class PG_process
{
public:
	int pid;
	int harmonics()
	{
		if (pid = fork())
		{
			return pid;
		}
		else
		{
			return 0;
		}
	}
	int Wait()
	{
		int stat;
		wait(&stat);
	}
};
class PG_TCP
{
public:
	int c_fd, l_fd, pid;
	int harmonics()
	{
		int pid, stat;
		if (pid = fork())
		{
			waitpid(pid, &stat, 0);
			return pid;
		}
		else
		{
			if (fork())
			{
				exit(0);
			}
			else return 0;
		}
	}
	void go()
	{
		struct sockaddr_in sin;
		struct sockaddr_in cin;
		
		socklen_t len;
		char buf[10000]; 
		char addr_p[INET_ADDRSTRLEN]; 
		int port = 7000;
		int n,r; 
       
		bzero(&sin, sizeof(sin)); 
		sin.sin_family = AF_INET; 
		sin.sin_addr.s_addr = INADDR_ANY; 
		sin.sin_port = htons(port);
    
		l_fd = socket(AF_INET, SOCK_STREAM, 0); 
		while(1)
		{
			r = bind(l_fd, (struct sockaddr *)&sin, sizeof(sin));
			cout << "bind: " << r << endl;
			if(r == 0)break;
			usleep(500000);
		}
		r = listen(l_fd, 10); 
		cout << "listen: " << r << endl;
		printf("waiting ...\n");
		while(1)
		{
			usleep(500000);
			c_fd = accept(l_fd, (struct sockaddr *) &cin, &len); 
			cout << "accept: " << c_fd << endl;
			if (pid = harmonics())
			{
				cout << "parent" << endl;
				close(c_fd);
			}
			else
			{
				close(l_fd);
				dup2(c_fd, 0);
				dup2(c_fd, 1);
				dup2(c_fd, 2);
				close(c_fd);
				return ;
			}
		}
	}

};
void pipe_exec(PG_pipe &Elie, PG_cmd &Tio, int from, int to)
{
	PG_process Rixia;
	int pid;
	if (from == to)
	{
		Tio.exec_seg(from);
		cerr << "error!" << endl;
		return;
	}
	int fd[2];
	pipe(fd);
	if(pid = Rixia.harmonics())
	{
		close(fd[1]);
		close(0);
		dup2(fd[0],0);
		close(fd[0]);
		Rixia.Wait();
		pipe_exec(Elie,Tio,from+1,to);
		return ;
	}
	else
	{
		close(fd[0]);
		close(1);
		dup2(fd[1],1);
		if (Tio.pipe_seg_flag[from])
		{
			close(2);
			dup2(fd[1],2);
		}
		close(fd[1]);
		Tio.exec_seg(from);
		exit(0);
	}
}
int main()
{
	PG_pipe Elie;
	PG_cmd Tio;
	PG_process Rixia;
	int seq_no = 0,pid;
	PG_TCP Noel;
	chdir(ROOT_DIC);
	Noel.go();
	welcome_msg();
	
	
	while (1)
	{
		cout << "% ";
		Tio.seq_no = ++seq_no;
		Tio.read();
		Tio.parse();
		//Tio.show();	
		if (Tio.exit_flag) exit(0);
		
		int pipe_to = 0;
		if(Tio.delay) 
		{
			pipe_to = seq_no + Tio.delay;
			Elie.connect(seq_no, pipe_to);
		}
		
		if (pid = Rixia.harmonics())
		{
			Elie.fix_main(seq_no);
			Rixia.Wait();
		}
		else
		{
			Elie.fix_stdin(seq_no);
			if (Tio.pipe_err_flag)
				Elie.fix_stdout(seq_no,1);
			else
				Elie.fix_stdout(seq_no,0);
			Elie.clean_pipe();
			if (Tio.redirect_to != "")
				Elie.redirect_to_file(Tio.redirect_to);
			
			if(Tio.pipe_seg.size() > 2)
			{
				pipe_exec(Elie, Tio, 0, Tio.pipe_seg.size()-2);
				exit(0);
				cerr << "failed to exit" << endl;
			}
			else
			{
				Tio.exec();
			}
		}
	}
}
