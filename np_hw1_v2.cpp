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
	map<string, int> pipe_read, pipe_write, time_stamp;
	map<string, int>::iterator iter,iter2;
	map<string, string> pipe_read_rev, pipe_write_rev;
	map<int, int> read_table;
	void show()
	{
		for (iter = pipe_read.begin(); iter != pipe_read.end(); ++iter)
			cout << "r_pipe " << iter->first << " " << iter->second << endl;
		for (iter = pipe_write.begin(); iter != pipe_write.end(); ++iter)
			cout << "w_pipe " << iter->first << " " << iter->second << endl;
	}
	void del(string q)
	{
		//iter = pipe_write.find(q);
		//if (iter != pipe_write.end()){close(iter->second);pipe_write.erase(iter);}
		iter = pipe_read.find(q);
		if (iter != pipe_read.end())
		{
			
			int tmp = read_table[iter->second];
			cerr << "deling" << iter->first << " aiming" << tmp << endl; 
			for (iter2 = pipe_write.begin(); iter2 != pipe_write.end(); ++iter2)
			{
				if(iter2->second == tmp)
				{
					cerr << "clean " << iter2->first << endl;
					close(iter2->second);
					pipe_write.erase(iter2);
				}
				
			}
			close(iter->second);
			pipe_read.erase(iter);
			show();
			cerr << "---" << endl;
		}
	}
	void rename(string a, string b)
	{
		iter = pipe_write.find(a);
		if (iter != pipe_write.end()){int t = iter->second; pipe_write.erase(iter); pipe_write[b] = t;}
		//else cerr << "ERROR!!" << endl;
	}
	void apply(string q)
	{
		//show();
		//cerr << "applying " << q << endl;
		iter = pipe_read.find(q);
		if (iter != pipe_read.end())
		{
			close(0);
			dup2(iter->second,0);
			close(iter->second);
			//cerr << getpid() << " dup " << iter->first << " id:" << iter->second << " to 0" << endl;
			pipe_read.erase(iter);
		}
		iter = pipe_write.find(q);
		if (iter != pipe_write.end())
		{
			close(1);
			dup2(iter->second,1);
			close(iter->second);
			//cerr << getpid() << " dup " << iter->first << " id:" << iter->second << " to 1" << endl;
			pipe_write.erase(iter);
		}

		//cerr << "after " << endl;
		//show();

	}
	void connect(string from, string to, int t_stamp)
	{
		iter = pipe_read.find(to);
		if(iter != pipe_read.end())
		{
			pipe_write[from] = read_table[pipe_read[to]];
			return ;
		}
		int fd[2];
		pipe(fd);
		pipe_read[to] = fd[0];
		pipe_write[from] = fd[1];
		read_table[fd[0]] = fd[1];
		//pipe_read_rev[to] = from;
		//pipe_write_rev[from] = to;
		//cout << "connect " << from << " to " << to << endl;
		time_stamp[from] = time_stamp[to] = t_stamp;
		//PIPE.push_back(pipe_unit(from,to));
	}
	void redirect_to_file(string q, string fn)
	{
		int fd = open(fn.c_str(),O_WRONLY | O_CREAT | O_TRUNC,700);
		close(1);
		dup2(fd,1);
		close(fd);
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
		pool[q] = pool[pid] + "/" + q;
		//cout << q << " " << pid << endl;
		if (t_pid = fork())
		{
			//cerr << "child " << pool[q] << endl;
			return t_pid;
		}
		else
		{
			//cerr << "parent " << pool[pid] << endl;
			pid = q;
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
		
		string ori = "cmd_" + i2s(seq_no);
		if(Tio.delay)
		{
			//cerr << "delay " << Tio.delay << endl;
			string t = "cmd_" + i2s(seq_no + Tio.delay);
			Elie.connect(ori, t, seq_no);
		}
		if (pid = Rixia.harmonics(ori))
		{
			Elie.del(ori);
			Rixia.Wait();
			//cerr << "finish waiting" << endl;
		}
		else
		{
			if(Tio.pipe_seg.size() > 2)
			{
				/*
					| A | B | C | D | E |
					      2   3   4   5
					0   1   2   3   4   5   size = 6 

				*/
				Elie.rename(Tio.process_name[0], Tio.process_name[Tio.process_name.size()-1]);
				for (int i = 0; i < Tio.process_name.size()-1; i++)
				{
					Elie.connect(Tio.process_name[i], Tio.process_name[i+1], seq_no);
				}
				for (int i = 0; i < Tio.process_name.size(); i++)
				{

					if (pid = Rixia.harmonics(ori))
					{
						Elie.del(Tio.process_name[i]);
						Rixia.Wait();
					}
					else
					{
						Elie.apply(Tio.process_name[i]);
						if(i == Tio.process_name.size()-1 && Tio.redirect_to != "")
							Elie.redirect_to_file(ori, Tio.redirect_to);
						Tio.exec_seg(i);
					}
				}
				exit(0);
			}
			else
			{
				Elie.apply(ori);
				if (Tio.redirect_to != "")
				{
					cerr << "detect red" << endl;
					Elie.redirect_to_file(ori, Tio.redirect_to);
				}
				Tio.exec();
			}
		}
	}

	
}
