#include "../include/Channel.hpp"

// ************************************************************************** //
//                       Constructors & Desctructors                          //
// ************************************************************************** //
Channel::Channel(std::string name) : _channelName(name), 
									 _channelPassword(""),
									 _channelTopic("Default"),
									 _channelIsInviteOnly(false),
									 _channelIsTopicRestricted(false),
									 _channelIsKeyProtected(false),
									 _channelIsOperatorRestricted(false),
									 _channelIsLimitRestricted(false),
									 _channelClientCount(0),
									 _channelClientLimit(CHAN_CLIENT_LIMIT) {}

Channel::~Channel(void)
{
	info("Channel " + _channelName + " destroyed");
}

// ************************************************************************** //
//                               Accessors                                    //
// ************************************************************************** //
bool	Channel::isInviteOnly(void) const {	return _channelIsInviteOnly; }

bool	Channel::isTopicRestricted(void) const { return _channelIsTopicRestricted; }

bool	Channel::isKeyProtected(void) const { return _channelIsKeyProtected; }

bool	Channel::isOperatorRestricted(void) const { return _channelIsOperatorRestricted; }

bool	Channel::isLimitRestricted(void) const { return _channelIsLimitRestricted; }

std::string	Channel::getName(void) const { return _channelName; }

std::string	Channel::getPassword(void) const { return _channelPassword; }

std::string	Channel::getTopic(void) const { return _channelTopic; }

size_t	Channel::getClientCount(void) const { return _channelClientCount; }

size_t	Channel::getClientLimit(void) const { return _channelClientLimit; }

std::vector<Client *>&  Channel::getClients(void) { return _channelClients; }

std::vector<Client *>&  Channel::getOperators(void) { return _channelOperators; }

void	Channel::setName(std::string &name) { _channelName = name; }

void	Channel::setPassword(std::string &password) { _channelPassword = password; }

void	Channel::setTopic(std::string &topic) { _channelTopic = topic; }

size_t	Channel::incClientCount(void) { return ++_channelClientCount; }

size_t	Channel::decClientCount(void)
{
	if (_channelClientCount > 0)
		return --_channelClientCount;
	else
	{
		error("Client count is already 0");
		return _channelClientCount;
	}
}

// ************************************************************************** //
//                             Public Functions                               //
// ************************************************************************** //
bool	Channel::isEmpty(void) const { return _channelClients.empty(); };

void	Channel::setModeI(std::string &mode, Client &client)
{
	_channelIsInviteOnly = (mode == "+i");
	broadcastToChannel(STD_PREFIX(client) + " MODE " + _channelName + " " + mode, NULL);
	if (mode == "+i")
	{
		info("Channel " + _channelName + " is now invite only: " + boolToString(_channelIsInviteOnly));
	}
	else if (mode == "-i")
	{
		info("Channel " + _channelName + " is no longer invite only");
	}
}

void	Channel::setModeT(std::string &mode, Client &client)
{
	_channelIsTopicRestricted = (mode == "+t");
	broadcastToChannel(STD_PREFIX(client) + " MODE " + _channelName + " " + mode, NULL);
	if (mode == "+t")
	{
		info("Channel " + _channelName + " is now topic restricted: " + boolToString(_channelIsTopicRestricted));
	}
	else if (mode == "-t")
	{
		info("Channel " + _channelName + " is now topic unrestricted");
	}
}

void	Channel::setModeK(std::vector<std::string> &msgData, Client &client)
{
	std::string mode = msgData[2];

	if (mode[0] == '+' && (msgData.size() != 4 || msgData[3] == ""))
	{
		//sendMSG(client.getFd(), ERR_BADCHANNELKEY(client, _channelName));
		return ;
	}
	if (mode[0] == '+')
	{
		_channelIsKeyProtected = true;
		_channelPassword = msgData[3];
	}
	else
	{
		_channelIsKeyProtected = false;
		_channelPassword = "";
	}
	info("Channel " + _channelName + " is now key protected: " + boolToString(_channelIsKeyProtected));
	broadcastToChannel(STD_PREFIX(client) + " MODE " + _channelName + " " + mode, NULL);
}

void	Channel::setModeO(std::string &mode, Client &client)
{
	_channelIsOperatorRestricted = (mode == "+o");
	broadcastToChannel(STD_PREFIX(client) + " MODE " + _channelName + " " + mode, NULL);
	if (mode == "+o")
	{
		info("Channel " + _channelName + " is now operator restricted: " + boolToString(_channelIsOperatorRestricted));
	}
	else if (mode == "-o")
	{
		info("Channel " + _channelName + " is now operator unrestricted");
	}
}

void	Channel::setModeL(std::vector<std::string> &msgData, Client &client)
{
	std::string mode = msgData[2];

	if (mode[0] == '+' && (msgData.size() != 4 || msgData[3] == ""))
	{
		//sendMSG(client.getFd(), ERR_BADCHANNELKEY(client, _channelName));
		return ;
	}
	if (mode[0] == '+')
	{
		_channelIsLimitRestricted = true;
		std::stringstream size(msgData[3]);
		size >> _channelClientLimit;
	}
	else
	{
		_channelIsLimitRestricted = false;
		_channelClientLimit = 0;
	}
	info("Channel " + _channelName + " is now limit restricted (currently " + sizeToString(_channelClientCount) + "/" +sizeToString(_channelClientLimit) + "): " + boolToString(_channelIsLimitRestricted));
	broadcastToChannel(STD_PREFIX(client) + " MODE " + _channelName + " " + mode, NULL);
}

bool	Channel::hasClient(Client* client) const
{
	for (std::vector<Client *>::const_iterator it = _channelClients.begin(); \
										   it != _channelClients.end(); ++it)
										   {
		if (*it == client)
			return true;
	}
	return false;
}

bool    Channel::isClientChanOp(Client* client) const
{
	for (std::vector<Client *>::const_iterator it = _channelOperators.begin(); \
										it != _channelOperators.end(); ++it)
										{
		if (*it == client)
			return true;
	}
	return false;
}

void	Channel::addChanOp(Client* client)
{
	//is this check and warning necessary? no but it feels more correct than 
	// granting rights to a client that is already an op
	if (client->isIRCOp())
	{
		return warning(client->username() + " is a global operator");
	}
	if (isClientChanOp(client))
	{
		return warning(client->username() + " is already an operator in channel " + _channelName);
	}
	_channelOperators.push_back(client);
	info(client->username() + " is now an operator in channel " + _channelName);
}

void	Channel::removeChanOp(Client* client)
{
	if (client->isIRCOp())
	{
		return warning(client->username() + " is a global operator");
	}
	if (!isClientChanOp(client))
	{
		return warning(client->username() + " is not an operator in channel " + _channelName);
	}
	for (std::vector<Client *>::iterator it = _channelOperators.begin(); it != _channelOperators.end(); ++it)
	{
		if (*it == client)
		{
			_channelOperators.erase(it);
			return;
		}
	}
	info(client->username() + " is no longer an operator in channel " + _channelName);
}

void	Channel::broadcastToChannel(std::string message, Client* client)
{
	if (isEmpty())
	{
		return warning("Channel is empty");
	}
	if (message.empty())
	{
		return warning("Empty message");
	}
	if (message.length() > 512)
	{
		return warning("Message too long");
	}
	for (std::vector<Client *>::const_iterator it = _channelClients.begin(); it != _channelClients.end(); ++it)
	{
		if (*it != client)
			sendMSG((*it)->getFd(), (message + "\r\n"));
	}
}