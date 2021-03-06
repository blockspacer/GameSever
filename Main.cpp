#include <iostream>
#include "./include/Main.h"
#include <map>
#include <string>


Log logop;
Mysql mysqlop;

map<string, int> usermap; // first-username second-userid
UserInfo userlist[MaxUserNum];
int get_pack[MaxUserNum];
int useri;
map<int, pair<int, int>> roommap; // first-roomid second<Aplayerid, Bplayerid>
int roomlist[MaxRoomNum];
int roomi;

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		cerr << "Error: You should input ./Main [port]" << endl;
		exit(-1);
	}
	int _port = atoi(argv[1]);
	if(_port < 21000 || _port > 21999)
	{
		cerr << "Error: The port number should between [21000 - 21999]" << endl;
		exit(-1);
	}

	daemon(0, 0);
	init();

	Server server;
	int serverstate = server.Initialize(_port);
	logop.Initialize(serverstate);
	if (serverstate == ERROR)
	{
		logop.Close();
		exit(-1);
	}
	server.dealpack = work;

	time_t last_check_time;
	time(&last_check_time);
	while (1)
	{
		check_client(last_check_time);

		if (server.MainActivity() == NOPACK)
			usleep(100000);
	}

	logop.Close();
	return 0;
}

int Mod(int &rhs, const int m)
{
	return rhs >= m ? rhs -= m : rhs;
}
string Transform(const char X, const char Y)
{
	string res = "";
	switch (Y)
	{
	case 0:
		res.append("A");
		break;
	case 1:
		res.append("B");
		break;
	case 2:
		res.append("C");
		break;
	case 3:
		res.append("D");
		break;
	case 4:
		res.append("E");
		break;
	case 5:
		res.append("F");
		break;
	case 6:
		res.append("G");
		break;
	case 7:
		res.append("H");
		break;
	case 8:
		res.append("I");
		break;
	case 9:
		res.append("J");
		break;
	default:
		break;
	}
	switch (X)
	{
	case 0:
		res.append("0");
		break;
	case 1:
		res.append("1");
		break;
	case 2:
		res.append("2");
		break;
	case 3:
		res.append("3");
		break;
	case 4:
		res.append("4");
		break;
	case 5:
		res.append("5");
		break;
	case 6:
		res.append("6");
		break;
	case 7:
		res.append("7");
		break;
	case 8:
		res.append("8");
		break;
	case 9:
		res.append("9");
		break;
	default:
		break;
	}
	return res;
}

void init()
{
	usermap.clear();
	roommap.clear();
	useri = 0;
	roomi = 0;
	memset(get_pack, 0, sizeof(get_pack));
	memset(roomlist, 0, sizeof(roomlist));
}

void check_client(time_t &last_check_time)
{
	time_t check_time;
	time(&check_time);
	if (check_time - last_check_time < TimeLimit)
		return;

	int drop_cnt = 0;
	int drop_list[MaxUserNum];
	for (int i = 0; i < MaxUserNum; ++i)
	{
		if (userlist[i].port == NoConnect)
			continue;
		if (get_pack[i])
			get_pack[i] = 0;
		else
			drop_list[drop_cnt++] = i;
	}

	for (int i = 0; i < drop_cnt; ++i)
	{
		if (userlist[drop_list[i]].roomid != NoRoom)
			left_room(userlist[drop_list[i]]);

		memcpy(logop.ip, userlist[drop_list[i]].ip, IPLength);
		logop.port = userlist[drop_list[i]].port;
		logop.Server_Log(logop._Missconnect);

		map<string, int>::iterator userit;
		for(userit = usermap.begin(); userit != usermap.end(); userit++)
			if(userit->second == drop_list[i])
			{
				usermap.erase(userit);
				break;
			}
		userlist[drop_list[i]].port = NoConnect;
		userlist[drop_list[i]].roomid = NoRoom;
	}
	last_check_time = check_time;
}

int work(Server *server, int nbytes, struct sockaddr_in client_addr, char *buff)
{
	strcpy(logop.ip,inet_ntoa(client_addr.sin_addr));
	logop.port = client_addr.sin_port;
	logop.nbytes = nbytes;
	logop.user.assign(buff + 2, UserNameLength);
	logop.status = buff[0];
	logop.op = buff[1];
	logop.Server_Log(logop._Recv);

	char status = logop.status;
	char op = logop.op;
	char *load = buff + 22;

	char sndbuffer[BuffLength];
	int sndbufferlength = 0;

	int TaihouDaisuki = 1;
	while (TaihouDaisuki)
	{
		map<string, int>::iterator userit;
		userit = usermap.find(logop.user);
		if(userit == usermap.end() && (status != LOGIN_STATUS || op != RCV_LOG_IN))
			return ERROR;

		if (userit != usermap.end())
			get_pack[userit->second] = 1;

		/* kick */
		if (userit != usermap.end() && userlist[userit->second].tbuff[0] == KickPack
			&& !strcmp(userlist[userit->second].ip, logop.ip) && userlist[userit->second].port == logop.port)
		{
			if(status == SPECIAL_STATUS)
			{
				user_logout(logop.user, logop.ip, logop.port, 1);
				sndbuffer[0] = SPECIAL_STATUS;
				sndbuffer[1] = SND_WAIT;
				sndbufferlength = 2;

				break;
			}
		 	sndbuffer[0] = SPECIAL_STATUS;
		 	sndbuffer[1] = SND_KICK;
		 	sndbufferlength = 2;

		 	break;
		}

		/* log in */
		if (status == LOGIN_STATUS)
		{
			sndbuffer[0] = LOGIN_STATUS;
			if (op == RCV_LOG_IN)
			{
				sndbuffer[1] = SND_LOG_IN;

				string password(load, PasswordLength);

				int sqlres;

				sqlres = mysqlop.check_user(logop.user);
				if (!sqlres)
				{
					sndbuffer[2] = ERROR;
					strcpy(sndbuffer + 3, "No such user.");
					sndbufferlength = 3 + strlen("No such user.") + 1;

					logop.Server_Log(logop._Connect, "failed(improper username).");
					break;
				}

				sqlres = mysqlop.check_password(logop.user, password);
				if (!sqlres)
				{
					sndbuffer[2] = ERROR;
					strcpy(sndbuffer + 3, "Wrong password.");
					sndbufferlength = 3 + strlen("Wrong password.") + 1;

					logop.Server_Log(logop._Connect, "failed(wrong password).");
					break;
				}

				int res = user_login(logop.user, logop.ip, logop.port);
				//if(res == ERROR) /* too many user, add in future (maybe) */
				if (res == REPEAT)
				{
					if (user_relogin(logop.user, logop.ip, logop.port) == OK)
					{
						sndbuffer[2] = OK;
						strcpy(sndbuffer + 3, "Your account has just logged out in another machine.");
						sndbufferlength = 3 + strlen("Your account has just logged out in another machine.") + 1;

						break;
					}

					userlist[userit->second].tbuff[0] = KickPack;

					sndbuffer[1] = SND_WAIT;
					sndbufferlength = 2;

					break;
				}

				sndbuffer[2] = OK;
				strcpy(sndbuffer + 3, "You have logged in successfully.");
				sndbufferlength = 3 + strlen("You have logged in successfully.") + 1;

				break;
			}
			else
				return ERROR;
		}

		/* search */
		if (status == SEARCH_STATUS)
		{
			sndbuffer[0] = SEARCH_STATUS;
			if (op == RCV_LOG_OUT)
			{
				if (userit == usermap.end() || strcmp(userlist[userit->second].ip, logop.ip) || userlist[userit->second].port != logop.port)
					return OK;

				user_logout(logop.user, logop.ip, logop.port);
				sndbuffer[1] = SND_WAIT;
				sndbufferlength = 2;

				break;
			}
			if (op == RCV_USER_LIST)
			{
				UserInfo &user = userlist[userit->second];

				if (user.tbuff[0] == InvitePack)
				{
					sndbuffer[1] = SND_IVT;
					memcpy(sndbuffer + 2, user.tbuff+1, UserNameLength);
					sndbufferlength = 2 + UserNameLength;

					break;
				}

				user.tbuff[0] = NonePack;

				int start_num = int(load[0]);
				int request_num = int(load[1]);
				string reslist[MaxUserNum];
				int totnum = 0;
				int resnum = get_user_list(logop.user, start_num, request_num, reslist, totnum);

				sndbuffer[1] = SND_USER_LIST;
				sndbuffer[2] = char(resnum);
				sndbuffer[3] = char(totnum);
				sndbufferlength = 4;
				for (int i = 0; i < resnum; ++i, sndbufferlength += UserNameLength)
					memcpy(sndbuffer + sndbufferlength, reslist[i].c_str(), UserNameLength);

				break;
			}
			if (op == RCV_BATTLE_REQ)
			{
				UserInfo &user = userlist[userit->second];
				string friendname(load, UserNameLength);
				map<string, int>::iterator friendit;
				friendit = usermap.find(friendname);

				if (user.tbuff[0] != NonePack)
				{
					if (user.tbuff[0] == WaitingPack)
					{
						if(friendit == usermap.end() || userlist[friendit->second].port == NoConnect)
						{
							user.tbuff[0] = InviteRequestPack;
							for (int i = 1; i <= 4; ++i)
								user.tbuff[i] = 'X';

							sndbuffer[1] = SND_ROOM_INFO;
							for (int i = 1; i <= 4; ++i)
								sndbuffer[i + 1] = user.tbuff[i];
							sndbufferlength = 6;
							break;
						}

						sndbuffer[1] = SND_WAIT;
						sndbufferlength = 2;

						break;
					}
					if (user.tbuff[0] == InviteRequestPack)
					{
						if (user.tbuff[1] != 'X')
						{
							int roomid = 0;
							for (int i = 1; i <= 4; ++i)
								roomid = roomid * 10 + user.tbuff[i] - '0';
						}

						sndbuffer[1] = SND_ROOM_INFO;
						for (int i = 1; i <= 4; ++i)
							sndbuffer[i + 1] = user.tbuff[i];
						sndbufferlength = 6;

						break;
					}
				}

				string Append(friendname);
				Append.append("[").append(userlist[friendit->second].ip).append("].");
				logop.Game_Log(logop._Invite, 0, Append);

				user.tbuff[0] = WaitingPack;
				userlist[friendit->second].tbuff[0] = InvitePack;
				memcpy(userlist[friendit->second].tbuff + 1, logop.user.c_str(), UserNameLength);

				sndbuffer[1] = SND_WAIT;
				sndbufferlength = 2;

				break;
			}
			if (op == RCV_HANDLE_IVT)
			{
				UserInfo &user = userlist[userit->second];

				if (user.tbuff[0] == InviteRequestPack)
				{
					sndbuffer[1] = SND_ROOM_INFO;
					for (int i = 1; i <= 4; ++i)
						sndbuffer[i + 1] = user.tbuff[i];
					sndbufferlength = 6;

					break;
				}

				string friendname(load, UserNameLength);
				int result = int(load[20]);

				map<string, int>::iterator friendit;
				friendit = usermap.find(friendname);

				string Append(friendname);
				Append.append("[").append(userlist[friendit->second].ip).append("].");
				if (result == OK)
					logop.Game_Log(logop._Accept, 0, Append);
				else
					logop.Game_Log(logop._Refuse, 0, Append);

				user.tbuff[0] = InviteRequestPack;
				userlist[friendit->second].tbuff[0] = InviteRequestPack;
				if (result == OK)
				{
					int flag = 0;
					for (int i = 0; i < MaxRoomNum; ++i, Mod(++roomi, MaxRoomNum))
						if (roomlist[roomi] == Available)
						{
							flag = 1;
							break;
						}
					// if(!flag)
					roomlist[roomi] = Full;

					for (int i = 4, roomid = roomi; i >= 1; --i, roomid /= 10)
						user.tbuff[i] = userlist[friendit->second].tbuff[i] = '0' + roomid % 10;
					roommap.insert(make_pair(roomi, make_pair(Empty, Empty)));

					join_room(logop.user, friendname, roomi);
				}
				else
				{
					for (int i = 1; i <= 4; ++i)
						user.tbuff[i] = userlist[friendit->second].tbuff[i] = 'X';
				}

				sndbuffer[1] = SND_ROOM_INFO;
				for(int i = 1; i <= 4; ++i)
					sndbuffer[i + 1] = user.tbuff[i];
				sndbufferlength = 6;

				break;
			}
			return ERROR;
		}

		/* room */
		if (status == ROOM_STATUS)
		{
			UserInfo &user = userlist[userit->second];

			sndbuffer[0] = ROOM_STATUS;
			if (op == RCV_LEAVE)
			{
				int res = left_room(user);
				if (res != NoRoom)
					logop.Game_Log(logop._Leave, res);

				user.tbuff[0] = NonePack;

				sndbuffer[1] = SND_WAIT;
				sndbufferlength = 2;

				break;
			}

			map<int,pair<int, int>>::iterator roomit;
			roomit = roommap.find(user.roomid);
			int opponent = user.side ? roomit->second.first : roomit->second.second;
			if (opponent == Empty || userlist[opponent].roomid != user.roomid)
			{
				sndbuffer[1] = SND_DROP;
				sndbufferlength = 2;
				break;
			}

			if (op == RCV_READY)
			{
				if (user.tbuff[0] == ReadyPack)
				{
					sndbuffer[1] = SND_READY;
					sndbufferlength = 2;

					break;
				}

				int ready = int(load[0]);
				int res = ready_operator(user, ready);
				if (res == GetMap)
				{
					user.tbuff[0] = userlist[opponent].tbuff[0] = ReadyPack;

					sndbuffer[1] = SND_READY;
					sndbufferlength = 2;

					break;
				}

				sndbuffer[1] = SND_STATE;
				sndbuffer[2] = userlist[opponent].plane == Ready;
				sndbufferlength = 3;

				break;
			}
			if (op == RCV_CHESS)
			{
				if (user.tbuff[0] == WaitPack)
				{
					sndbuffer[1] = SND_WAIT;
					sndbufferlength = 2;

					break;
				}
				if (user.tbuff[0] == StartPack)
				{
					sndbuffer[1] = SND_START;
					sndbuffer[2] = user.side;
					sndbufferlength = 3;

					break;
				}

				user.tbuff[0] = WaitPack;

				char p[4 * PlaneNum];
				memcpy(p, load, 4 * PlaneNum);
				int res = start_operator(user, p);

				if (res == Start)
				{
					map<int,pair<int, int>>::iterator roomit;
					roomit = roommap.find(user.roomid);

					userlist[roomit->second.first].tbuff[0] = userlist[roomit->second.second].tbuff[0] = StartPack;
					logop.Game_Log(logop._Start, roomit->first);

					sndbuffer[1] = SND_START;
					sndbuffer[2] = user.side;
					sndbufferlength = 3;

					break;
				}
				sndbuffer[1] = SND_WAIT;
				sndbufferlength = 2;

				break;
			}
			return ERROR;
		}

		/* game */
		if (status == BATTLE_STATUS)
		{
			UserInfo &user = userlist[userit->second];
			map<int,pair<int, int>>::iterator roomit;
			roomit = roommap.find(user.roomid);
			int opponent = user.side ? roomit->second.first : roomit->second.second;

			sndbuffer[0] = BATTLE_STATUS;
			if (op == RCV_WAIT)
			{
				if (opponent == Empty || userlist[opponent].roomid != user.roomid)
				{
					sndbuffer[1] = SND_DROP;
					sndbufferlength = 2;
					left_room(user);
				}
				else if (user.tbuff[0] == ClickPack)
				{
					sndbuffer[1] = SND_CLICK_OP;
					sndbuffer[2] = user.tbuff[1]; //X
					sndbuffer[3] = user.tbuff[2]; //Y
					sndbuffer[4] = user.tbuff[3]; //Res
					sndbufferlength = 5;
				}
				else if (user.tbuff[0] == CheckPack)
				{
					sndbuffer[1] = SND_CHECK_OP;
					sndbuffer[2] = user.tbuff[1]; //X0
					sndbuffer[3] = user.tbuff[2]; //Y0
					sndbuffer[4] = user.tbuff[3]; //X1
					sndbuffer[5] = user.tbuff[4]; //Y1
					sndbuffer[6] = user.tbuff[5]; //Res
					sndbuffer[7] = user.tbuff[6]; //End
					sndbufferlength = 8;
				}
				else
				{
					sndbuffer[1] = SND_WAIT;
					sndbufferlength = 2;
				}
				break;
			}
			if (op == RCV_CLICK)
			{
				char X = load[0];
				char Y = load[1];
				int res = click_operator(user, X, Y);

				user.tbuff[0] = NonePack;
				userlist[opponent].tbuff[0] = ClickPack;
				userlist[opponent].tbuff[1] = X;
				userlist[opponent].tbuff[2] = Y;
				userlist[opponent].tbuff[3] = res;

				sndbuffer[1] = SND_CLICK_RES;
				sndbuffer[2] = res;
				sndbufferlength = 3;

				break;
			}
			if (op == RCV_CHECK)
			{
				char X0 = load[0];
				char Y0 = load[1];
				char X1 = load[2];
				char Y1 = load[3];
				int res = check_operator(user, X0, Y0, X1, Y1);

				user.tbuff[0] = NonePack;
				userlist[opponent].tbuff[0] = CheckPack;
				userlist[opponent].tbuff[1] = X0;
				userlist[opponent].tbuff[2] = Y0;
				userlist[opponent].tbuff[3] = X1;
				userlist[opponent].tbuff[4] = Y1;
				userlist[opponent].tbuff[5] = res == Right ? 1 : 0;
				userlist[opponent].tbuff[6] = res == GameEnd ? 1 : 0;

				sndbuffer[1] = SND_CHECK_RES;
				sndbuffer[2] = ((res == Right) || (res == GameEnd));
				sndbuffer[3] = res == GameEnd;
				sndbufferlength = 4;

				break;
			}
			return ERROR;
		}
	}

	logop.status = sndbuffer[0];
	logop.op = sndbuffer[1];
	logop.nbytes = server->Send(sndbuffer, client_addr, sndbufferlength);
	logop.Server_Log(logop._Send);
	return OK;
}

int user_login(string username, const char *ip, const int port)
{
	map<string, int>::iterator userit;
	userit = usermap.find(username);

	if (userit != usermap.end()) // has login
	{
		if (!strcmp(userlist[userit->second].ip, ip) && userlist[userit->second].port == port)
		{
			if (userlist[userit->second].roomid != NoRoom)
				return REPEAT;
			return OK;
		}
		return REPEAT;
	}

	int flag = 0;
	for (int i = 0; i < MaxUserNum; ++i, Mod(++useri, MaxUserNum))
	{
		if (userlist[useri].port != NoConnect)
			continue;
		flag = 1;
		break;
	}

	if (!flag)
		return ERROR;
	usermap.insert(make_pair(username, useri));

	UserInfo &user = userlist[useri];
	memcpy(user.ip, ip, IPLength);
	user.port = port;
	user.kick = 0;
	user.roomid = NoRoom;
	user.tbuff[0] = NonePack;

	logop.Server_Log(logop._Connect, username + " login successfully.");
	return OK;
}
int user_relogin(string username, const char *ip, const int port)
{
	map<string, int>::iterator userit;
	userit = usermap.find(username);

	if (userit == usermap.end())
		return ERROR;

	if (userlist[userit->second].kick == 2)
		return OK;
	if (!userlist[userit->second].kick)
		return ERROR;

	UserInfo &user = userlist[useri];
	memcpy(user.ip, ip, IPLength);
	user.port = port;
	user.kick = 2;
	user.roomid = NoRoom;
	user.tbuff[0] = NonePack;

	logop.Server_Log(logop._Reconnect, username + " relogin successfully");
	return OK;
}

int user_logout(string username, const char *ip, const int port, const int kick)
{
	map<string, int>::iterator userit;
	userit = usermap.find(username);

	if (userit == usermap.end())
		return ERROR;
	if (userlist[userit->second].kick == 1)
		return OK;

	if (kick)
	{
		userlist[userit->second].kick = 1;
		if (userlist[userit->second].roomid != NoRoom)
			left_room(userlist[userit->second]);
		logop.Server_Log(logop._Disconnect, username + " was kicked off.");
	}
	else
	{
		userlist[userit->second].port = NoConnect;
		userlist[userit->second].roomid = NoRoom;
		usermap.erase(userit->first);

		logop.Server_Log(logop._Disconnect, username + " log out successfully");
	}

	return OK;
}

int get_user_list(string username, const int start_num, const int request_num, string *reslist, int &totnum)
{
	totnum = 0;
	int avanum = 0;

	map<string, int>::iterator userit;
	for (userit = usermap.begin(); userit != usermap.end(); ++userit)
	{
		if (avanum == start_num)
			break;
		if (userit->first == username)
			continue;

		++totnum;
		if (userlist[userit->second].roomid == NoRoom)
			++avanum;
	}
	int res = 0;
	for (; userit != usermap.end(); ++userit)
	{
		if(userit->first == username)
			continue;
		if(userlist[userit->second].roomid != NoRoom)
			continue;

		reslist[res++] = userit->first;
		if (res == request_num)
			break;
	}

	return res;
}
int join_room(string userA, string userB, const int roomid)
{
	map<string, int>::iterator itA;
	map<string ,int>::iterator itB;
	itA = usermap.find(userA);
	itB = usermap.find(userB);

	if (itA == usermap.end() || itB == usermap.end())
		return ERROR;
		
	UserInfo &uA = userlist[itA->second];
	UserInfo &uB = userlist[itB->second];

	if (uA.roomid == roomid)
		return OK;

	map<int, pair<int, int>>::iterator roomit;
	roomit = roommap.find(roomid);
	if (roomit == roommap.end())
		return NoExist;

	time_t Time;
	time(&Time);
	int f = Time & 1;
	if(f)
	{
		roomit->second.first = itA->second;
		uA.side = 0;
		roomit->second.second = itB->second;
		uB.side = 1;
	}
	else
	{
		roomit->second.first = itB->second;
		uA.side = 1;
		roomit->second.second = itA->second;
		uB.side = 0;
	}

	uA.roomid = uB.roomid = roomid;
	uA.plane = uB.plane = Unready;

	string Append;
	Append.append(userA).append(" and ").append(userB).append(" have joined.");
	logop.Game_Log(logop._Join, roomid, Append);
	return OK;
}
int left_room(UserInfo &user)
{
	if (user.roomid == NoRoom)
		return NoRoom;

	map<int, pair<int, int>>::iterator roomit;
	roomit = roommap.find(user.roomid);
	if (roomit == roommap.end())
		return ERROR;

	int tmproomid = user.roomid;
	user.roomid = NoRoom;
	if (!user.side)
		roomit->second.first = Empty;
	else
		roomit->second.second = Empty;

	if ((roomit->second.first == Empty) & (roomit->second.second == Empty))
	{
		roomlist[roomit->first] = Available;
		roommap.erase(roomit);
	}
	return tmproomid;
}

int ready_operator(UserInfo &user, const int isReady)
{
	user.plane = isReady ? Ready : Unready;
	if (isReady)
		logop.Game_Log(logop._Ready, user.roomid);
	else
		logop.Game_Log(logop._Unready, user.roomid);

	map<int, pair<int, int>>::iterator roomit;
	roomit = roommap.find(user.roomid);
	if (roomit == roommap.end())
		return ERROR;

	int opponent = user.side ? roomit->second.first : roomit->second.second;

	return (user.plane == Ready && opponent != Empty && userlist[opponent].plane == Ready) ? GetMap : Waiting;
}
int start_operator(UserInfo &user, const char *p)
{
	if (user.plane != PlaneNum)
	{
		if (!(user.plane == Ready))
			return ERROR;

		user.plane = PlaneNum;
		for (int i = 0; i < PlaneNum; ++i)
		{
			for (int j = 0; j <= 1; ++j)
				user.planeX[j][i] = *(p++), user.planeY[j][i] = *(p++);
			user.planeflag[i] = 1;
		}

		for (int i = 0; i < ChessSize * ChessSize; ++i)
				user.A[i] = NoPlane;
		for (int i = 0; i < PlaneNum; ++i)
			draw_plane(user.A, user.planeX[0][i], user.planeY[0][i], user.planeX[1][i], user.planeY[1][i]);

		string Map = "";
		for (int i = 0; i < ChessSize; ++i)
		{
			int base = i * ChessSize;
			for (int j = 0; j < ChessSize; ++j)
			{
				char tA = user.A[base + j] + '0';
				Map.append(&tA, 1);
			}
			Map.append("\n");
		}
		logop.Game_Log(logop._GetMap, user.roomid, Map);
	}

	map<int, pair<int, int>>::iterator roomit;
	roomit = roommap.find(user.roomid);
	if (roomit == roommap.end())
		return ERROR;
	int opponent = user.side ? roomit->second.first : roomit->second.second;

	return (userlist[opponent].plane == Ready) ? GetMap : Start;
}
int click_operator(UserInfo &user, const char X, const char Y)
{
	map<int, pair<int, int>>::iterator roomit;
	roomit = roommap.find(user.roomid);
	if (roomit == roommap.end())
		return ERROR;
	UserInfo &opponent = user.side ? userlist[roomit->second.first] : userlist[roomit->second.second];

	int pos = ChessSize * Y + X;
	if (opponent.A[pos] < 0)
		return -opponent.A[pos];

	string Append = "";
	Append.append("(").append(Transform(X, Y)).append("),  the result is ");
	if (opponent.A[pos] == NoPlane)
		Append.append("Empty.");
	else if (opponent.A[pos] == HitPlane)
		Append.append("Plane.");
	else
		Append.append("Aircraft nose.");
	opponent.A[pos] = -opponent.A[pos];
	logop.Game_Log(logop._Operate, roomit->first, Append);
	return -opponent.A[pos];
}
int check_operator(UserInfo &user, const char X0, const char Y0, const char X1, const char Y1)
{
	map<int, pair<int, int>>::iterator roomit;
	roomit = roommap.find(user.roomid);
	if (roomit == roommap.end())
		return ERROR;
	UserInfo &opponent = user.side ? userlist[roomit->second.first] : userlist[roomit->second.second];

	int flag = -1;
	for (int i = 0; i < PlaneNum; ++i)
		if (opponent.planeX[0][i] == X0 && opponent.planeY[0][i] == Y0 && opponent.planeX[1][i] == X1 && opponent.planeY[1][i] == Y1)
		{
			flag = i;
			break;
		}
	string Append = "";
	Append.append("(").append(Transform(X0, Y0)).append(")-(").append(Transform(X1, Y1)).append("), the result is ");
	if (flag == -1)
	{
		Append.append("wrong");
		logop.Game_Log(logop._Check, roomit->first, Append);
		return Wrong;
	}
	if (!opponent.planeflag[flag])
		return Right;

	Append.append("right");
	logop.Game_Log(logop._Check, roomit->first, Append);
	fill_plane(opponent.A, X0, Y0, X1, Y1);
	if (!(--opponent.plane))
	{
		user.plane = Unready;
		opponent.plane = Unready;
		logop.Game_Log(logop._Finish, roomit->first);
		return GameEnd;
	}
	else
		return Right;
}
void draw_plane(char *A, const char X0, const char Y0, const char X1, const char Y1)
{
	int basepos = Y0 * ChessSize + X0;
	int k, pos;

	if (X0 == X1)
		k = Y0 < Y1 ? 0 /* up */ : 1 /* down */;
	else
		k = X0 < X1 ? 2 /* left */ : 3 /* right */;

	A[basepos] = CritialHit;
	for (int i = 1; i < PlaneSize; ++i)
	{
		pos = basepos + deltaX[k][i] + deltaY[k][i] * ChessSize;
		A[pos] = HitPlane;
	}
}
void fill_plane(char *A, const char X0, const char Y0, const char X1, const char Y1)
{
	int basepos = Y0 * ChessSize + X0;
	int k, pos;
	if (X0 == X1)
		k = Y0 < Y1 ? 0 /* up */ : 1 /* down */;
	else
		k = X0 < X1 ? 2 /* left */ : 3 /* right */;
	for (int i = 0; i < PlaneSize; ++i)
	{
		pos = basepos + deltaX[k][i] + deltaY[k][i] * ChessSize;
		A[pos] = -A[pos];
	}
}
