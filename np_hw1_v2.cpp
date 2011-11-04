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
		//cerr << "get" << p_table[q].fd[0] << " " << p_table[q].fd[1] << endl;
	}
	void close2(int q)
	{
		//cerr << "pid " << getpid() << " close " << q << endl;
		close(q);
		fd_table[q] = 0;
	}
	void dup22(int a,int b)
	{
		//cerr << "pid " << getpid() << " dup " << a << " to " << b <<endl;
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
		if (iter == p_table.end())
		{
			//cerr << "fix_stdin cant find pipe " << q << endl;
			return;
		}
		close2(0);
		dup22(iter->second.fd[0], 0);
		close2(iter->second.fd[0]);

	}
	void fix_stdout(int q)
	{
		int aim = relation[q];
		iter = p_table.find(aim);
		if (iter == p_table.end())
		{
			//cerr << "fix_stdout cant find pipe " << aim << endl;
			return;
		}
		close2(1);
		dup22(iter->second.fd[1], 1);
		close2(iter->second.fd[1]);
		
	}
	void fix_main(int q)
	{
		iter = p_table.find(q);
		if (iter == p_table.end())
		{
			//cerr << "fix_main cant find pipe " << q << endl;
			return;
		}
		close2(iter->second.fd[0]);
		close2(iter->second.fd[1]);
	}
	void connect(int a, int b)
	{
		cerr << "connect " << a << " to " << b << endl;
		relation[a] = b;
		create(b);
		
	}
	int chk_connect(int q){return relation[q];}
	void redirect_to_file(string q, string fn)
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
	vector<string> list,process_name;
	vector<int> pipe_seg;
	int delay, delay_type;
	int seq_no;
	string prefix;
	string redirect_from, redirect_to; 
	int size;
	PG_cmd()
	{
		size = 0;
		redirect_from = "";
		redirect_to = "";
		prefix = "cmd_";
		
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
		
		for (int i = 0; i < list.size(); i++)
		{
			if(list[i] == ">")
			{
				redirect_to = list[i+1];
				list.erase(list.begin() + i); list.erase(list.begin() + i);
				cerr << "Redirect to " << redirect_to << endl;
			}
		} 
		
		int end = list.size() - 1;
		if(list[end][0] =='|' || list[end][0] == '!')
		{
			list[end][0]=' ';
			delay = s2i(list[end]);
			list.erase(list.begin() + end);
		}
		//cerr << "after erase" << endl;
		pipe_seg.clear();
		pipe_seg.push_back(-1);
		for (int i = 1; i < list.size(); i++)
		{	
			if (list[i] == "|")
			{
				pipe_seg.push_back(i);
			}			
		}
		pipe_seg.push_back(list.size());
		
		string seq_no2 = i2s(seq_no);
		process_name.clear();
		process_name.push_back(prefix + seq_no2);
		for (int i = 2; i < pipe_seg.size(); i++)
		{	
			process_name.push_back(prefix + seq_no2 + "_" + i2s(i));	
		}
		if (0)
		{
			cout << "seg_size " << pipe_seg.size() << endl;
			cout << "process_name_size " << process_name.size() << endl;
			for (int i = 0; i < process_name.size(); i++)
				cout << process_name[i] << " " << pipe_seg[i]+1 << " " << pipe_seg[i+1]-1 << endl;
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
		cerr << "this is " << getpid() << endl;
		char **tmp = new char*[1001];
		int i, cnt;
		if(from > to)return;
		//cerr << "from " << from << " to " << to << endl;
		for (i = from,cnt = 0; i <= to; i++,cnt++)
		{
			tmp[cnt] = new char[1001];
			strcpy(tmp[cnt], list[i].c_str());
		}
		tmp[i] = NULL;
		//for (int i = 0; tmp[i] != NULL; i++)cerr << "execvp " << tmp[i] << endl;
		execvp(tmp[0], tmp);
	}
	void exec() {exec(0,list.size()-1);}
	void exec_seg(int q){exec(pipe_seg[q]+1, pipe_seg[q+1]-1);}

};
class PG_process
{
public:
	map<string, string> pool;

	string pid;
	PG_process()
	{
		pid = "init";
		pool["init"] = "init";
	}
	int harmonics(string q)
	{
		int t_pid;
		//pool[q] = pool[pid] + "/" + q;
		//cout << q << " " << pid << endl;
		if (t_pid = fork())
		{
			//cerr << "child " << pool[q] << endl;
			return t_pid;
		}
		else
		{
			//cerr << "parent " << pool[pid] << endl;
			//pid = q;
			return 0;
		}
	}
	int harmonics(string q,int q2)
	{
		harmonics(q + "_" +i2s(q2));
	}
	int Wait()
	{
		int stat;
		wait(&stat);
	}
	string relation(){return pool[pid];}
	
	
	
};
void pipe_exec(PG_pipe &Elie, PG_cmd &Tio, int from, int to)
{
	PG_process Rixia;
	int pid;
	if (from == to)
	{
		Tio.exec_seg(from);
	}
	int fd[2];
	pipe(fd);
	if(pid = Rixia.harmonics("pipe"))
	{
		close(fd[1]);
		close(0);
		dup2(fd[0],0);
		close(fd[0]);
		Rixia.Wait();
		pipe_exec(Elie,Tio,from+1,to);
	}
	else
	{
		close(fd[0]);
		close(1);
		dup2(fd[1],1);
		close(fd[1]);
		Tio.exec_seg(from);
	}
}
int main()
{
	PG_pipe Elie;
	PG_cmd Tio;
	PG_process Rixia;
	int seq_no = 0,pid;
	while (1)
	{
		cout << "% ";
		Tio.seq_no = ++seq_no;
		Tio.read();
		Tio.parse();
		//Tio.show();	
		
		int pipe_to = 0;
		if(Tio.delay) 
		{

			pipe_to = seq_no + Tio.delay;
			Elie.connect(seq_no, pipe_to);
		}
		if (pid = Rixia.harmonics(i2s(seq_no)))
		{
			Elie.fix_main(seq_no);
			Elie.show();
			Rixia.Wait();

		}
		else
		{
			Elie.fix_stdin(seq_no);
			Elie.fix_stdout(seq_no);
			Elie.clean_pipe();
			Elie.show();
			if (Tio.redirect_to != "")
			{
				cerr << "detect red" << endl;
				//Elie.redirect_to_file(ori, Tio.redirect_to);
			}
				
			if(Tio.pipe_seg.size() > 2)
			{
				pipe_exec(Elie, Tio, 0, Tio.pipe_seg.size()-1);
			}
			else
			{
				Tio.exec();
			}
		}
	}

	
}
