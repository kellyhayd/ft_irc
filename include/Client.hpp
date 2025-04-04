#pragma once
#include "irc.hpp"

class Channel;
class ChannelManager;

class Client
{
	private:
		std::string				_username;
		std::string 			_nickname;
		std::string 			_fullname;
		std::string 			_hostname;
		std::string 			_IP;
		pollfd					_socket;

		bool					_isRegistered;
		bool 					_isIRCOp;

		std::vector<Channel*>   _clientChannels;

	public:

		/* construcotrs & destructors */
		Client(pollfd &clientSocket);
		~Client(void);

		std::string msgBuffer;
        
        /* accessors */
		std::vector<Channel*>  getClientChannels() const;
		int 			getFd(void) const;
		struct pollfd 	getSocket(void) const;
		
		void			setIP(std::string IP);
		void 			setFullName(std::string &fullname);
		void 			setNickname(std::string &nickname);
		void 			setUsername(std::string &username);
		void 			setHostname(std::string &hostname);
		void			setRegistered(bool status);
		void 			setIRCOp(bool status);

		
		std::string 	username(void) const;
		std::string 	nickname(void) const;
		std::string 	hostname(void) const;

        bool    		isOperator(void) const;
		bool			isRegistered(void) const;
		bool 			isIRCOp(void) const;

		
		/* member functions */
		void    		joinChannel(ChannelManager& manager, std::string channelName);
        void    		leaveChannel(ChannelManager& manager, std::string channelName);
		

};
