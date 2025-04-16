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
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, msgData[0]));
		return warning("Insufficient parameters for MODE command");
	}
	// silently ignore user modes
	if (_server.getClientByNick(msgData[1]))
		return ;

	std::string channelName = msgData[1];
	Channel* chan = _manager.getChanByName(channelName);
	if (!chan) {
		sendMSG(client.getFd(), ERR_NOSUCHCHANNEL(client, channelName));
		return warning("Channel " + channelName + " does not exist");
	}
	if (chan->isClientChanOp(&client) || client.isIRCOp())
		_manager.setChanMode(msgData, client);
	else
	{
		sendMSG(client.getFd(), ERR_CHANOPPROVSNEEDED(client, channelName));
		sendMSG(client.getFd(), SERVER_NAME + " " + client.nickname() + " NOTICE :You are not a channel operator\r\n");
		return warning(client.nickname() + " is not an operator in channel " + channelName);
	}
}

void MsgHandler::handleTOPIC(std::string &msg, Client &client)
{
	std::string topic = split(msg, ':').back();
	std::string channelName = split(msg, ' ').at(1);
	Channel* channel = _manager.getChanByName(channelName);

	if (!channel) {
		sendMSG(client.getFd(), ERR_NOSUCHCHANNEL(client, channelName));
		return warning("Channel " + channelName + " does not exist");
	}
	if (!channel->hasClient(&client))
	{
		sendMSG(client.getFd(), ERR_NOTONCHANNEL(client, channelName));
		return warning(client.nickname() + " is not an operator in channel " + channelName);
	}
	if (channel->isTopicRestricted() && !(channel->isClientChanOp(&client) || client.isIRCOp()))
	{
		sendMSG(client.getFd(), ERR_CHANOPPROVSNEEDED(client, channelName));
		return warning(client.nickname() + " is not an operator in channel " + channelName);
	}
	channel->setTopic(topic, client.nickname());
	info(client.nickname() + " changed topic of channel " + channel->getName() + " to: " + topic);
	channel->broadcast(RPL_TOPIC(client, channel->getName(), topic));
}

void MsgHandler::handlePRIVMSG(std::string &msg, Client &client)
{
	const std::vector<std::string> &trailing = split(msg, ':');
	const std::vector<std::string> &command = split(trailing[0], ' ');

	if (trailing.size() < 2 || command.size() < 2) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, "PRIVMSG"));
		return warning("Insufficient parameters for PRIVMSG command");
	}
	std::string channelName = command.at(1);
	std::string message = trailing.at(1);

	if (msg.find("!quote") != std::string::npos)
	{
		_manager.forwardPrivateMessage(channelName, msg, client);
		handleQuote(channelName, client);
		return;
	}

	_manager.forwardPrivateMessage(channelName, message, client);
}

void MsgHandler::handleKILL(std::string &msg, Client &killer)
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
			clientChannels[i]->broadcast(KILL(killer, victim, clientChannels[i], reasonToKill));
			_manager.removeFromChannel(clientChannels[i]->getName(), victim);
		}
		sendMSG(victim.getFd(), QUITKILLEDBY(victim, killer, reasonToKill));
		_server.disconnectClient(&victim);
		return ;
	}
}

void MsgHandler::handleQUIT(std::string &msg, Client &client)
{
	std::vector<std::string> msgData = split(msg, ':');
	if (msgData.size() < 2) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, msgData[0]));
		return warning("Insufficient parameters for JOIN command");
	}
	std::string message = (msgData[1].empty()) ? "No reason given" : msgData[1];

	std::vector<Channel*>& clientChannels = client.getClientChannels();
	for (size_t i = 0; i < clientChannels.size(); i++)
	{
		clientChannels[i]->broadcast(QUIT(client, message));
		_manager.removeFromChannel(clientChannels[i]->getName(), client);
	}
	_server.disconnectClient(&client);
}

void MsgHandler::handleDIE(Client &client)
{
	if (!client.isIRCOp())
		return sendMSG(client.getFd(), ERR_NOPRIVILAGES(client));

	clients_t &allClients = _server.getClients();
	for (clients_t::iterator it = allClients.begin(); it != allClients.end(); ++it)
	{
		Client &c = *it->second;
		std::vector<Channel *> clientChannels = c.getClientChannels();
		for (size_t i = 0; i < clientChannels.size();i++)
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
			nickname = msgData[1] + intToString(i++);
		}
	}
	else if ((_server.getClientByNick(nickname))) {
		return sendMSG(client.getFd(), ERR_NICKNAMEINUSE(client, msgData[1]));
	}
	client.setNickname(nickname);
}


void	MsgHandler::handleQuote(const std::string& channelTarget, Client& client)
{
	if (!_server.getQuoteBot()->initiateConnection(_server))
	{
		info("Failed to connect to QuoteBot API");
		return;
	}
	_server.getQuoteBot()->setRequesterClient(&client);
	_server.getQuoteBot()->setRequesterChannel(channelTarget);
}

void MsgHandler::handlePART(std::vector<std::string> &msgData, Client &client)
{
	if (msgData.size() < 2) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, msgData[0]));
		return warning("Insufficient parameters for MODE command");
	}
	_manager.removeFromChannel(msgData[1], client);
}

void MsgHandler::handlePASS(std::vector<std::string> &msgData, Client &client)
{
	if (msgData.size() < 2) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, msgData[0]));
		return warning("Insufficient parameters for MODE command");
	}
	_server.validatePassword(msgData[1], client);
}

void MsgHandler::handleOPER(std::vector<std::string> &msgData, Client &client)
{
	if (msgData.size() < 3) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, msgData[0]));
		return warning("Insufficient parameters for OPER command");
	}
	std::string &nickname = msgData[1];
	std::string &password = msgData[2];

	_server.validateIRCOp(nickname, password, client);
}

void MsgHandler::handleJOIN(std::vector<std::string> &msgData, Client &client)
{
	if (msgData.size() < 2) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, msgData[0]));
		return warning("Insufficient parameters for JOIN command");
	}
	if (msgData[1][0] != '#') {
		sendMSG(client.getFd(), ERR_BADCHANMASK(client, msgData[1]));
		return warning("Channel " + msgData[1] + " does not exist");
	}
	std::string &channelName = msgData[1];
	std::string channelKey = (msgData.size() == 3) ? msgData[2] : "";

	_manager.addToChannel(channelName, channelKey, client);
}

void MsgHandler::handleINVITE(std::vector<std::string> &msgData, Client &client)
{
	if (msgData.size() < 3) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, msgData[0]));
		return warning("Insufficient parameters for INVITE command");
	}
	std::string &channelName = msgData[1];
	std::string &nickname = msgData[2];
  	
	_manager.inviteClient(channelName, nickname, client);
}

void MsgHandler::handleKICK(std::string &msg, Client &client)
{
	std::string channelName, userToKick, reason;
	const std::vector<std::string> &msgData = split(msg, ':');
	const std::vector<std::string> &names = split(msgData[0], ' ');

	if (msgData.size() < 2 || names.size() < 3) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, "KICK"));
		return warning("Insufficient parameters for KICK command");
	}
	channelName = names[1];
	userToKick = names[2];
	reason = (msgData.size() > 1) ? msgData[1] : "No reason given";

	_manager.kickFromChannel(channelName, userToKick, reason, client);
}
void MsgHandler::handleUSER(std::string &msg, Client &client)
{
	std::string username, hostname, IP, fullName;
	const std::vector<std::string> &msgData = split(msg, ':');
	const std::vector<std::string> &names = split(msgData[0], ' ');

	if (msgData.size() < 2 || names.size() < 4) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, "KICK"));
		return warning("Insufficient parameters for KICK command");
	}

	client.assignUserData(username, hostname, IP, fullName);
}

void	MsgHandler::handleSENDFILE(std::string &msg, Client& client)
{
	std::cout << PURPLE << "handleSENDFILE" << RESET << std::endl; // DEBUG

	std::istringstream ss(msg);
	std::string sendFileCommand, recipientNick, filePath;
	getline(ss, sendFileCommand, ' ');
	getline(ss, recipientNick, ' ');
	getline(ss, filePath);
	if (filePath.empty() || recipientNick.empty())
	{
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, "SENDFILE"));
		return warning("Insufficient parameters for SENDFILE command");
	}
	else if (_server.getClientByNick(recipientNick) == NULL || client.nickname() == recipientNick)
	{
		sendMSG(client.getFd(), ERR_NOSUCHNICK(client, recipientNick));
		return warning("Recipient " + recipientNick + " does not exist");
	}
	size_t pos = filePath.find_last_of('/');
	if (pos == std::string::npos)
	{
		sendMSG(client.getFd(), ERR_BADFILEPATH(client, filePath));
		return warning("File " + filePath + " does not exist");
	}
	std::string	fileName = filePath.substr(filePath.find_last_of('/') + 1);
	std::fstream	file(filePath.c_str());
	if (file.fail())
	{
		sendMSG(client.getFd(), ERR_BADFILEPATH(client, fileName));
		return warning("File " + filePath + " does not exist");
	}
	File* fileInfo = _server.prepareUpload(&client, recipientNick, fileName);

	if (fileInfo)
	{
		client.setIsUploading(true);
		client.setCurrentUploadId(fileInfo->transferId);
		sendMSG(client.getFd(), "SENDFILE ACCEPTED ID=" + fileInfo->transferId + " Ready to receive data for " + fileName + "\r\n");
		info("Sent ready message to " + client.nickname() + " for transfer " + fileInfo->transferId);
	}
	else
	{
		sendMSG(client.getFd(), "ERROR :Server could not prepare the file transfer. Please try again later.");
		warning("SENDFILE: Failed to prepare upload for " + client.nickname());
	}
}

void	MsgHandler::handleUploadData(Client &client, const char* dataBuffer, size_t dataSize)
{
	if (dataSize <= 0)
		return;

	std::string transferId = client.getCurrentUploadId();
	if (transferId.empty()) {
		warning("Client " + client.nickname() + " is in uploading state but has no currentUploadId!");
		client.setIsUploading(false);
		sendMSG(client.getFd(), "ERROR :Internal server error during upload. Transfer cancelled.");
		return;
	}

	File* transferInfo = _server.getTransferById(transferId);
	if (!transferInfo) {
		warning("Client " + client.nickname() + " sent upload data for non-existent transfer ID: " + transferId);
		client.setIsUploading(false);
		client.setCurrentUploadId("");
		sendMSG(client.getFd(), "ERROR :Your file transfer was not found or may have timed out. Transfer cancelled.");
		return;
	}

	if (transferInfo->state != PENDING_UPLOAD && transferInfo->state != UPLOADING) {
		 warning("Client " + client.nickname() + " sent upload data for transfer " + transferId + " which is not in uploading state (" + intToString(transferInfo->state) + ")");
		 return;
	}

	if(transferInfo->state == PENDING_UPLOAD)
		transferInfo->state = UPLOADING;

	transferInfo->lastActivityTime = std::time(0);

	if (transferInfo->tempFileStream.is_open()) {
		transferInfo->tempFileStream.write(dataBuffer, dataSize);
		if (transferInfo->tempFileStream.fail()) {
			error("Failed to write to temporary file: " + transferInfo->tempFilePath + " for transfer " + transferId);
			transferInfo->state = FAILED;
			client.setIsUploading(false);
			client.setCurrentUploadId("");
			sendMSG(client.getFd(), "ERROR :Server failed to write file data. Transfer cancelled.");
			_server.finalizeTransfer(transferId, true);
			return;
		}
	} else {
		error("Temporary file stream is not open for transfer " + transferId);
		transferInfo->state = FAILED;
		client.setIsUploading(false);
		client.setCurrentUploadId("");
		sendMSG(client.getFd(), "ERROR :Internal server error (file stream). Transfer cancelled.");
		_server.finalizeTransfer(transferId, false);
		return;	
	}

	if (dataSize < 1024)
	{
		info("Upload complete for transfer ID: " + transferId + " (" + transferInfo->fileName);
		transferInfo->tempFileStream.close();
		transferInfo->state = UPLOAD_COMPLETE;
		client.setIsUploading(false);
		client.setCurrentUploadId("");

		Client* recipient = _server.getClientByNick(transferInfo->recipientNick);
		if (recipient) {
			transferInfo->recipientClient = recipient;
			std::string notifyMsg = " :File '" + transferInfo->fileName + " received from " + client.nickname() +
									". Use: GETFILE " + client.nickname() + " " + transferInfo->fileName + "\r\n";
			sendMSG(recipient->getFd(), notifyMsg);
			info("Notified recipient " + recipient->nickname() + " about completed upload " + transferId);
		}
		else
			warning("Recipient " + transferInfo->recipientNick + " is no longer online for transfer " + transferId + ". File stored temporarily.");
	}
}

void MsgHandler::handleGETFILE(std::string &msg, Client& client)
{
	std::istringstream iss(msg);
	std::string command, senderNick, filename;

	if (!(iss >> command >> senderNick >> filename)) {
		sendMSG(client.getFd(), ERR_NEEDMOREPARAMS(client, "GETFILE"));
		warning("GETFILE: Insufficient parameters from " + client.nickname());
		return;
	}
	std::string remaining;
	if (iss >> remaining) {
		sendMSG(client.getFd(), "ERROR :Too many parameters for GETFILE. Use: GETFILE <sender> <filename>");
		warning("GETFILE: Too many parameters from " + client.nickname());
		return;
	}

	std::string transferId = senderNick + "_" + filename;
	info("GETFILE request from " + client.nickname() + " for transfer ID: " + transferId);

	File* transferInfo = _server.getTransferById(transferId);

	if (!transferInfo) {
		sendMSG(client.getFd(), "ERROR :No such file transfer pending from " + senderNick + " for file " + filename);
		warning("GETFILE: Transfer not found: " + transferId + " requested by " + client.nickname());
		return;
	}

	if (transferInfo->recipientNick != client.nickname()) {
		 sendMSG(client.getFd(), "ERROR :You are not the intended recipient for the file '" + filename + "' from " + senderNick);
		 warning("GETFILE: Permission denied for " + client.nickname() + " requesting transfer " + transferId + " intended for " + transferInfo->recipientNick);
		return;
	}

	if (transferInfo->state != UPLOAD_COMPLETE) {
		 std::string errMsg = "ERROR :File transfer for '" + filename + "' is not ready for download. State: ";
		 errMsg += intToString(transferInfo->state);
		 sendMSG(client.getFd(), errMsg);
		 warning("GETFILE: Transfer " + transferId + " not ready for download, requested by " + client.nickname() + ". State: " + intToString(transferInfo->state));
		return;
	}

	 if (client.isDownloading() || client.isUploading()) {
		  sendMSG(client.getFd(), "ERROR :You are already busy with another file transfer. Please wait or cancel.");
		  warning("GETFILE: Client " + client.nickname() + " is busy with another transfer.");
		  return;
	 }

	info("GETFILE request validated for " + client.nickname() + ". Initiating download for " + transferId);

	transferInfo->recipientClient = &client;
	transferInfo->lastActivityTime = std::time(0);
	transferInfo->state = PENDING_DOWNLOAD;
	client.setIsDownloading(true);
	client.setCurrentDownloadId(transferId);

	sendMSG(client.getFd(), "SUCCESS :Starting download for file '" + filename + "' from " + senderNick + "...");
	_server.startSendingFile(transferId);
}

void MsgHandler::respond(std::string &msg, Client &client)
{
	std::vector<std::string> msgData = split(msg, ' ');

	switch (getCommandType(msgData[0]))
	{
		case PASS: handlePASS(msgData, client);
			break ;
		case OPER: handleOPER(msgData, client); 
			break ;
		case JOIN: handleJOIN(msgData, client);
			break ;
		case PART: handlePART(msgData, client);
			break ;
		case INVITE: handleINVITE(msgData, client);
			break ;
		case KICK: handleKICK(msg, client);
			break ;
		case USER: handleUSER(msg, client);
			break ;
		case QUIT: handleQUIT(msg, client);
			break ;
		case NICK: handleNICK(msgData, client);
			break ;
		case MODE: handleMODE(msgData, client);
			break ;
		case TOPIC: handleTOPIC(msg, client);
			break ;
		case KILL: handleKILL(msg, client);
			break ;
		case DIE: handleDIE(client);
			break ;
		case PING: sendMSG(client.getFd(), PONG);
			break ;
		case PRIVMSG: handlePRIVMSG(msg, client);
			break ;
		case SENDFILE: handleSENDFILE(msg, client);
			break ;
		case GETFILE: handleGETFILE(msg, client);
			break ;
		case UNKNOWN:
			break ;
	}
}

void	MsgHandler::receiveMessage(Client &client)
{
	char		buffer[1024];
	ssize_t bytes_read = read(client.getFd(), buffer, sizeof(buffer) - 1);
	if (bytes_read <= 0) {
		if (client.isUploading()) {
			 File* transfer = _server.getTransferById(client.getCurrentUploadId());
			 if(transfer) {
				 warning("Client " + client.nickname() + " disconnected during upload of " + transfer->fileName);
				 transfer->state = FAILED;
				 _server.finalizeTransfer(transfer->transferId, true);
			 }
		}
		return _server.disconnectClient(&client);
	}
	buffer[bytes_read] = '\0';

	std::cout << buffer; // for testing only

	if (client.isUploading())
		handleUploadData(client, buffer, bytes_read);
	else
	{
		if (!strcmp(buffer, "\r\n"))
			return ;
		
		client.msgBuffer += buffer;
		size_t i;
		while ((i = client.msgBuffer.find("\r\n")) != std::string::npos)
		{
			std::string message = client.msgBuffer.substr(0, i);
			client.msgBuffer.erase(0, i + 2);
			if (!client.isRegistered() && split(message, ' ').front() == "NICK")
			{
				error("Invalid or no password: client disconnected.");
				sendMSG(client.getFd(), ERR_PASSWDMISMATCH(client));
				_server.disconnectClient(&client);
				return ;
			}
			respond(message, client);  // maybe take PASS out of this funciton
		}
	}
}
