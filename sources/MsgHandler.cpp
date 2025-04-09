#include "../include/MsgHandler.hpp"
#include "../include/irc.hpp"

// ************************************************************************** //
//                       Constructors & Desctructors                          //
// ************************************************************************** //

MsgHandler::MsgHandler(Server &server, ChannelManager &manager)
	: _server(server), _manager(manager) {};

MsgHandler::~MsgHandler() {};


// ************************************************************************** //
//                             Public Functions                               //
// ************************************************************************** //


void MsgHandler::handleMODE(std::vector<std::string> &msgData, Client &client)
{
	if (msgData.size() < 2) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client));
		return warning("Insufficient parameters for MODE command");
	}

	std::string channelName = msgData[1];
	Channel* chan = _manager.getChanByName(channelName);
	if (!chan)
	{
		sendMSG(client.getFd(), ERR_NOSUCHCHANNEL(client, channelName));
		return warning("Channel " + channelName + " does not exist");
	}
	if (chan->isClientChanOp(&client) || client.isIRCOp())
		_manager.setChanMode(msgData, client);
	else
	{
		sendMSG(client.getFd(), ERR_CHANOPPROVSNEEDED(client, channelName));
		return warning(client.nickname() + " is not an operator in channel " + channelName);
	}
}

void	MsgHandler::handleTOPIC(std::string &msg, Client &client)
{
	std::vector<std::string> msgData = split(msg, ' ');
	Channel* chan = _manager.getChanByName(msgData[1]);

	if (!chan)
	{
		sendMSG(client.getFd(), ERR_NOSUCHCHANNEL(client, msgData[1]));
		return warning("Channel " + msgData[1] + " does not exist");
	}
	if (msgData.size() > 2)
	{
		if (chan->isTopicRestricted() && (chan->isClientChanOp(&client) || client.isIRCOp()))
		{
			chan->setTopic(msgData[2]);
			info(client.nickname() + " changed topic of channel " + chan->getName() + " to: " + chan->getTopic());
		}
		else if (!chan->isTopicRestricted())
		{
			chan->setTopic(msgData[2]);
			info(client.nickname() + " changed topic of channel " + chan->getName() + " to: " + chan->getTopic());
		}
		else
		{
			sendMSG(client.getFd(), ERR_CHANOPPROVSNEEDED(client, msgData[1]));
			warning(client.nickname() + " is not an operator in channel " + msgData[1]);
		}
	}
	else
		sendMSG(client.getFd(), RPL_TOPIC(client, chan->getName(), chan->getTopic()));
}

void	MsgHandler::forwardPrivateMessage(std::string &msg, Client &client)
{
	const std::vector<Channel*>& clientChannels = client.getClientChannels();
	std::map<std::string, Channel*> allChannels = _manager.getChannels();

	if (clientChannels.empty())
		return error("Client not in any channel, IRSSI applying PRIVMSG incorrectly");

	if (msg.empty())
        return warning("Empty PRIVMSG received");

    std::vector<std::string> tokens = split(msg, ' ');
    if (tokens.size() < 2 || tokens[0] != "PRIVMSG")
        return warning("Invalid PRIVMSG format");

    const std::string& channel = tokens[1];
    if (channel.empty() || channel[0] != '#')
	{
		sendMSG(client.getFd(), ERR_NOSUCHCHANNEL(client, channel));
        return warning("PRIVMSG channel is missing or invalid");
	}

	std::map<std::string, Channel*>::iterator it = allChannels.find(channel);
	if (it == allChannels.end())
	{
		sendMSG(client.getFd(), ERR_NOSUCHCHANNEL(client, channel));
		return warning("Channel " + channel + " does not exist in the server");
	}
	Channel* chan = it->second;
	if (chan->isEmpty())
		return warning("Channel is empty");

	chan->broadcastToChannel(STD_PREFIX(client) + " " + msg, &client);
}

void	MsgHandler::handleKILL(std::string &msg, Client &killer)
{
	if (!killer.isIRCOp())
	{
		sendMSG(killer.getFd(), ERR_NOPRIVILAGES(killer));
		return ;
	}

	std::istringstream ss(msg);
	std::string killCommand, userToKill, reasonToKill;
	getline(ss, killCommand, ' ');
	getline(ss, userToKill, ' ');
	getline(ss, reasonToKill);

	Client *client = _server.getClientByNick(userToKill);
	if (client)
	{
		Client& victim = *client;

		std::vector<Channel*> clientChannels = client->getClientChannels();
		for (size_t i = 0; i < clientChannels.size(); i++)
		{
			clientChannels[i]->broadcastToChannel(KILL(killer, victim, clientChannels[i], reasonToKill), NULL);
			sendMSG(client->getFd(), RPL_NOTINCHANNEL((*client), clientChannels[i]->getName()));
		}
		sendMSG(client->getFd(), QUIT((*client), killer, reasonToKill));
		_server.disconnectClient(*client);
		return ;
	}
}

void	MsgHandler::handleDIE(Client &client)
{
	if (!client.isIRCOp())
		return sendMSG(client.getFd(), ERR_NOPRIVILAGES(client));

	clients_t &allClients = _server.getClients();
	for (clients_t::iterator it = allClients.begin(); it != allClients.end(); ++it)
	{
		Client &c = *it->second;
		std::vector<Channel*> clientChannels = c.getClientChannels();
		for (size_t i = 0; i < clientChannels.size(); i++)
		{
			sendMSG(c.getFd(), RPL_NOTINCHANNEL(c, clientChannels[i]->getName()));
		}
		sendMSG(c.getFd(), DIE(c));
	}
	info("DIE command received. Server shutting down...");
	_server.shutdown();
}

void MsgHandler::handleNICK(std::vector<std::string> &msgData, Client &client)
{
	if (msgData.size() < 2) {
		return sendMSG(client.getFd(), ERR_NONICKNAMEGIVEN(client));
	}

	std::string nickname = msgData[1];
	if (client.nickname() == "undefined")
	{
		int i = 1;
		while (_server.getClientByNick(nickname))
		{		
			std::ostringstream intTag;
			intTag << i++;
			nickname = msgData[1] + intTag.str();
		}
	}
	else if ((_server.getClientByNick(nickname))) {
		return sendMSG(client.getFd(), ERR_NICKNAMEINUSE(client, msgData[1]));
	}
	client.setNickname(nickname);
}

void	MsgHandler::respond(std::string &msg, Client &client)
{
	std::vector<std::string> msgData = split(msg, ' ');

	switch (getCommandType(msgData[0]))
	{
		case PASS: _server.validatePassword(msgData[1], client);
			break ;
		case QUIT: _server.disconnectClient(client);
			break ;
		case OPER: _server.validateIRCOp(msgData, client);
			break ;
		case JOIN: _manager.addToChannel(client, msgData[1]);
			break ;
		case PART: _manager.removeFromChannel(msgData[1], client);
			break ;
		case INVITE: _manager.inviteClient(msgData[1], msgData[2], client);
			break ;
		case KICK: _manager.kickFromChannel(msg, client);
			break ;
		case NICK: handleNICK(msgData, client); ;
			break ;
		case MODE: handleMODE(msgData, client);
			break ;
		case TOPIC: handleTOPIC(msg, client);
			break ;
		case KILL: handleKILL(msg, client);
			break ;
		case DIE: handleDIE(client);
			break ;
		case USER: client.assignUserData(msg);
			break ;
		case PING: sendMSG(client.getFd(), PONG);
			break ;
		case PRIVMSG: forwardPrivateMessage(msg, client);
			break ;
		case UNKNOWN:
			break ;
	}
}

bool	MsgHandler::badPassword(std::string &message, Client &client)
{
	std::vector<std::string> msgData = split(message, ' ');
	if (msgData[0] == "NICK")
	{
		sendMSG(client.getFd(), ERR_PASSWDMISMATCH(client));
		_server.disconnectClient(client);
		return (true);
	}
	return (false);
}

void	MsgHandler::receiveMessage(Client &client)
{
	char		buffer[1024];
	
	ssize_t bytes_read = read(client.getFd(), buffer, sizeof(buffer) - 1);
	if (bytes_read <= 0)
	{
		_server.disconnectClient(client);
		return ;
	}
	buffer[bytes_read] = '\0';
	std::cout << buffer; // for testing only 

	client.msgBuffer += buffer;
	size_t i;
	while ((i = client.msgBuffer.find("\r\n")) != std::string::npos)
	{
		std::string message = client.msgBuffer.substr(0, i);
		client.msgBuffer.erase(0, i + 2);
		if (!client.isRegistered() && badPassword(message, client))
			return ;
		respond(message, client);
	}
}
