#include "../include/irc.hpp"
#include <sys/stat.h>

// ************************************************************************** //
//                       Constructors & Desctructors                          //
// ************************************************************************** //
Server::Server(int port, std::string &password)
{
	_port = port;
	_password = password;
	_quoteBot = new QuoteBot();
	_file = NULL;
	parseOpersConfigFile("./include/opers.config");
	
	pollfd		listeningSocket;
	listeningSocket.fd = socket(AF_INET, SOCK_STREAM, 0);
	listeningSocket.events = POLLIN;
	listeningSocket.revents = 0;
	_sockets.push_back(listeningSocket);
	
	sockaddr_in	serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(_port);

	int opt = 1;
	setsockopt(_sockets[0].fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (fcntl(_sockets[0].fd, F_SETFL, O_NONBLOCK) < 0) {
		error("fcntl failure");
		exit(-1);
	}
	bind(_sockets[0].fd, (struct sockaddr *)(&serverAddr), sizeof(serverAddr));
	listen(_sockets[0].fd, 10);
}

Server::~Server()
{
	for (unsigned int i = 0; i < _sockets.size(); i++)
		close(_sockets[i].fd);

	for (clients_t::iterator it = _clients.begin(); it != _clients.end(); it++)
		delete it->second;
	delete _quoteBot;
	delete _file;
}

// ************************************************************************** //
//                               Accessors                                    //
// ************************************************************************** //
clients_t&	Server::getClients(void) { return (_clients); }

std::string Server::getPassword(void) { return (_password); }

std::map<std::string,std::string> Server::getOpers(void) { return (_opers); }

Client*	Server::getClientByUser(std::string& username) const
{
	for (clients_t::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->nickname() == username)
			return (it->second);
	}
	return (NULL);
}

Client*	Server::getClientByNick(std::string& nickname) const
{
	for (clients_t::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->nickname() == nickname)
			return (it->second);
	}
	return (NULL);
}

QuoteBot*	Server::getQuoteBot(void) { return (_quoteBot); }

File*	Server::getFile(void) { return (_file); }

// ************************************************************************** //
//                             Private Functions                              //
// ************************************************************************** //

pollfd	Server::_makePollfd(int fd, short int events, short int revents)
{
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = revents;
	return pfd;
}

// ************************************************************************** //
//                             Public Functions                               //
// ************************************************************************** //

void	Server::validatePassword(std::string &password, Client &client)
{
	if (password == getPassword())
	{
		client.setRegistered(true);
		sendMSG(client.getFd(), RPL_REGISTERED(client));
	}
	else
		sendMSG(client.getFd(), ERR_PASSWDMISMATCH(client));
}

void	Server::validateIRCOp(std::string &nickname, std::string &password, Client &client)
{
	std::map<std::string, std::string> allowedOpers = getOpers();
	std::map<std::string, std::string>::iterator it = allowedOpers.find(nickname);

	if (it == allowedOpers.end()){
		return sendMSG(client.getFd(), ERR_NOOPERHOST(client));
	}
	if (it->second != password){
		return sendMSG(client.getFd(), ERR_PASSWDMISMATCH(client));
	}
	info(client.nickname() + " set as operator");
	client.setIRCOp(true);
	sendMSG(client.getFd(), RPL_YOUROPER(client));
}

void	Server::shutdown() { _running = false; }

void	Server::parseOpersConfigFile(const char *fileName)
{
	std::ifstream file;
	
	file.open(fileName, std::ios::in);
	if (!file.is_open())
	{
		std::cerr << "Failed to open file: " << fileName << std::endl;
		return;
	}

	std::string line;
	while(std::getline(file, line))
	{
		std::vector<std::string> oper = split(line, ' ');
		_opers.insert(std::pair<std::string, std::string>(oper[0], oper[1]));
	}
}

void	Server::addclient(pollfd &clientSocket)
{
	Client *newClient = new Client(clientSocket);
	_clients.insert(client_pair_t (clientSocket.fd, newClient));
	_sockets.push_back(newClient->getSocket());
}

void	Server::disconnectClient(Client *client)
{
	info(client->nickname() + " disconnected");

	for (unsigned int i = 0; i < _sockets.size(); i++)
	{
		if (_sockets[i].fd == client->getFd())
		{
			close(client->getFd());
			_clients.erase(client->getFd());
			_sockets.erase(_sockets.begin() + i);
			delete client;
			return ;
		}
	}
}

void	Server::handleNewConnectionRequest(void)
{
	sockaddr_in		clientAddr;
	pollfd			clientSocket;
	unsigned int	addrLen = sizeof(clientAddr);
	
	clientSocket.fd = accept(_sockets[0].fd, (sockaddr *)&clientAddr, &addrLen);
	clientSocket = _makePollfd(clientSocket.fd, POLLIN | POLLHUP | POLLERR, 0);
	if (clientSocket.fd < 0)
	{
		close(_sockets[0].fd);
		return error("New socket creation failed.");
	}
	sendMSG(clientSocket.fd, "CAP * LS : \r\n");
	addclient(clientSocket);
  	info("New client connected with fd: " + intToString(clientSocket.fd));
}

void	Server::SIGINTHandler(int signum)
{
	if (instance == NULL)
		return error("Server instance is NULL, cannot handle signal");
	if (signum == SIGINT)
	{
		info("SIGINT received, shutting server down...");
		_running = false;
	}
	for (clients_t::iterator it = instance->_clients.begin(); it != instance->_clients.end(); ++it)
	{
		Client &c = *it->second;
		std::vector<Channel *> clientChannels = c.getClientChannels();
		for (size_t i = 0; i < clientChannels.size();i++)
		{
  		  	sendMSG(c.getFd(), RPL_NOTINCHANNEL(c, clientChannels[i]->getName()));
		}
		sendMSG(c.getFd(), DIE(c));
	}
	instance->shutdown();
}

void Server::addApiSocket(pollfd &api_pfd) {
	_sockets.push_back(api_pfd);
	info("API socket fd " + intToString(api_pfd.fd) + " added for polling.");
}

void	Server::removeApiSocket(int fd) {
	for (size_t i = 0; i < _sockets.size(); ++i) {
		if (_sockets[i].fd == fd) {
			_sockets.erase(_sockets.begin() + i);
			info("API socket fd " + intToString(fd) + " removed from polling.");
			break;
		}
	}
}

bool	Server::handleApiEvent(pollfd apiFd)
{
	if (apiFd.revents & (POLLHUP | POLLERR | POLLNVAL))
		_quoteBot->closeApiConnection(*this);
	else if ((apiFd.revents & POLLOUT) != 0 && _quoteBot->apiState() != RECEIVING)
	{
		if (_quoteBot->apiState() == CONNECTING)
			_quoteBot->handleApiConnectionResult(*this);
		else if (_quoteBot->apiState() == SENDING)
			_quoteBot->sendHttpRequest(*this);
	}
	else if ((apiFd.revents & POLLIN) && _quoteBot->apiState() == RECEIVING)
		_quoteBot->handleAPIMessage(*this);
	else if (apiFd.revents != 0)
		return false;
	return true;
}

void	Server::setFile(File *file)
{
	if (_file != NULL)
		delete _file;
	_file = file;
}

std::string	Server::generateTempFilePath(const std::string& sender, const std::string& fileName) {
	std::string safeFilename = fileName;
	std::replace(safeFilename.begin(), safeFilename.end(), '/', '_');
    std::replace(safeFilename.begin(), safeFilename.end(), '\\', '_');

	std::time_t now = std::time(0);
	std::ostringstream oss;
	mkdir("/ircTranfers", 0700);
	oss << "/ircTranfers/" << sender << "_" << now << "_" << safeFilename;
	return oss.str();
}

File* Server::prepareUpload(Client* sender, const std::string& recipient, const std::string& fileName)
{
	if (!sender) return NULL;

	std::string transferId = sender->nickname() + "_" + fileName;
	if (_activeTransfers.count(transferId)) {
		warning("Transfer with ID: " + transferId + " already exists.");
		 sendMSG(sender->getFd(), "ERROR :Transfer for " + fileName + " already in progress.");
		return NULL;
	}

	File* newTransfer = new File();
	newTransfer->transferId = transferId;
	newTransfer->senderNick = sender->nickname();
	newTransfer->recipientNick = recipient;
	newTransfer->fileName = fileName;
	newTransfer->state = PENDING_UPLOAD;
	newTransfer->senderClient = sender;
	newTransfer->recipientClient = NULL;
	newTransfer->lastActivityTime = std::time(0);
	newTransfer->tempFilePath = generateTempFilePath(sender->nickname(), fileName);

	newTransfer->tempFileStream.open(newTransfer->tempFilePath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!newTransfer->tempFileStream.is_open()) {
		error("Failed to open temporary file for writing: " + newTransfer->tempFilePath);
		delete newTransfer;
		return NULL;
	}

	info("Prepared upload for transfer ID: " + transferId + ", temp file: " + newTransfer->tempFilePath);
	_activeTransfers[transferId] = newTransfer;
	return newTransfer;
}

File* Server::getTransferById(const std::string& transferId) {
	transfersMap::iterator it = _activeTransfers.find(transferId);
	if (it != _activeTransfers.end()) {
		return it->second;
	}
	return NULL;
}

void Server::finalizeTransfer(const std::string& transferId, bool deleteTempFile) {
	transfersMap::iterator it = _activeTransfers.find(transferId);
	if (it != _activeTransfers.end()) {
		File* transferInfo = it->second;

		info("Finalizing transfer ID: " + transferId);

		if (transferInfo->tempFileStream.is_open()) {
			transferInfo->tempFileStream.close();
		}

		if (deleteTempFile) {
			if (std::remove(transferInfo->tempFilePath.c_str()) != 0) {
				warning("Could not remove temporary file: " + transferInfo->tempFilePath);
			} else {
				 info("Removed temporary file: " + transferInfo->tempFilePath);
			}
		}
		if(transferInfo->senderClient)
		{
			transferInfo->senderClient->setIsUploading(false);
			transferInfo->senderClient->setCurrentUploadId("");
		}
		if(transferInfo->recipientClient)
		{
			transferInfo->recipientClient->setIsDownloading(false);
			transferInfo->recipientClient->setCurrentDownloadId("");
		}
		_activeTransfers.erase(it);
		delete transferInfo;
	} else
		warning("Attempted to finalize non-existent transfer ID: " + transferId);
}

void Server::checkTransferTimeouts() {
	time_t currentTime = std::time(0);
	const long timeoutSeconds = 180;
	std::vector<std::string> timedOutIds;

	for (transfersMap::iterator it = _activeTransfers.begin(); it != _activeTransfers.end(); ++it) {
		if (it->second->state != UPLOADING && it->second->state != DOWNLOADING)
		{
			 if (currentTime - it->second->lastActivityTime > timeoutSeconds)
			 {
				 warning("Transfer timed out: " + it->first);
				 timedOutIds.push_back(it->first);
				 if(it->second->senderClient)
				 	sendMSG(it->second->senderClient->getFd(), "ERROR :Transfer for " + it->second->fileName + " timed out.");
				 if(it->second->recipientClient)
				 	sendMSG(it->second->recipientClient->getFd(), "ERROR :Transfer for " + it->second->fileName + " from " + it->second->senderNick + " timed out.");
			 }
		}
	}
	for(size_t i = 0; i < timedOutIds.size(); ++i) {
		finalizeTransfer(timedOutIds[i], true);
	}
}

void	Server::startSendingFile(const std::string& transferId)
{
	File* transferInfo = getTransferById(transferId);
	if (!transferInfo)
		return warning("startSendingFile: Transfer ID not found: " + transferId);
	if (transferInfo->state != PENDING_DOWNLOAD && transferInfo->state != UPLOADING)
		return warning("startSendingFile: Transfer " + transferId + " is not in state PENDING_DOWNLOAD. State: " + intToString(transferInfo->state));

	Client* recipient = transferInfo->recipientClient;
	if (!recipient)
	{
		warning("startSendingFile: Recipient " + (recipient ? recipient->nickname() : "null") + " not found or disconnected for transfer " + transferId);
		transferInfo->state = FAILED;
		finalizeTransfer(transferId, true);
		return;
	}
	std::ifstream tempFileReadStream(transferInfo->tempFilePath.c_str(), std::ios::in | std::ios::binary);
	if (!tempFileReadStream.is_open()) {
		error("startSendingFile: Failed to open temporary file for reading: " + transferInfo->tempFilePath);
		transferInfo->state = FAILED;
		recipient->setIsDownloading(false);
		recipient->setCurrentDownloadId("");
		 sendMSG(recipient->getFd(), NOTICE(recipient->nickname(), "ERROR :Server could not read the uploaded file. Transfer failed."));
		finalizeTransfer(transferId, true);
		return;
	}

	info("Starting to send file " + transferInfo->fileName + " (ID: " + transferId + ") to " + recipient->nickname());
	transferInfo->state = DOWNLOADING;
	transferInfo->lastActivityTime = std::time(0);

	const size_t chunkSize = 4096;
	char buffer[chunkSize];
	bool sendError = false;

	while (tempFileReadStream.read(buffer, chunkSize) || tempFileReadStream.gcount() > 0) {
		ssize_t bytesToSend = tempFileReadStream.gcount();
		ssize_t totalSent = 0;

		while (totalSent < bytesToSend) {
			ssize_t sent = send(recipient->getFd(), buffer + totalSent, bytesToSend - totalSent, MSG_NOSIGNAL);

			if (sent <= 0) {
				if (sent == 0) {
					warning("startSendingFile: Client " + recipient->nickname() + " closed connection during transfer " + transferId);
				} else {
					perror("send error");
					warning("startSendingFile: Send error to client " + recipient->nickname() + " during transfer " + transferId + ", errno: " + intToString(errno));
				}
				sendError = true;
				break;
			}
			totalSent += sent;
		}
		if (sendError) {
			break;
		}
		 transferInfo->lastActivityTime = std::time(0);
	}
	tempFileReadStream.close();

	if (sendError) {
		error("Failed to send file " + transferInfo->fileName + " to " + recipient->nickname());
		transferInfo->state = FAILED;
		recipient->setIsDownloading(false);
		recipient->setCurrentDownloadId("");
		 sendMSG(recipient->getFd(), NOTICE(recipient->nickname(), "ERROR :Connection error during file download. Transfer failed."));
		finalizeTransfer(transferId, true);
	} else {
		info("Successfully sent file " + transferInfo->fileName + " (ID: " + transferId + ") to " + recipient->nickname());
		transferInfo->state = TRANSFER_COMPLETE;
		recipient->setIsDownloading(false);
		recipient->setCurrentDownloadId("");
		 sendMSG(recipient->getFd(), NOTICE(recipient->nickname(), "SUCCESS :File download '" + transferInfo->fileName + "' complete."));
		finalizeTransfer(transferId, true);
	}
}

void	Server::setBot()
{
	info("Setting bot...");

	int	sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1)
		return warning("Failed to create socket pair for bot");

	fcntl(sv[0], F_SETFL, O_NONBLOCK);
	fcntl(sv[1], F_SETFL, O_NONBLOCK);
	_quoteBot->setBotSocketFd(sv[0]);
	pollfd botSocket = _makePollfd(sv[1], POLLIN | POLLHUP | POLLERR, 0);

	Client *bot = new Client(botSocket);
	std::string botNickname = std::string(CYAN) + "QuoteBot" + GREEN;
	std::string botUsername = "QuoteBotAPI";
	std::string botHostname = "api.forismatic.com";
	bot->setNickname(botNickname);
	bot->setUsername(botUsername);
	bot->setHostname(botHostname);
	bot->setRegistered(true);
	bot->setBot(true);
	
	_sockets.push_back(botSocket);
	_clients.insert(client_pair_t (sv[1], bot));
	info("Bot " + botNickname + " created with fd: " + intToString(botSocket.fd));
}

void	Server::run(void)
{
	ChannelManager  manager(*this);
	MsgHandler		msg(*this, manager);

	Server::instance = this;
	signal(SIGINT, SIGINTHandler);
	info("Running...");

	setBot();
	while (_running)
	{
		int serverActivity = poll(_sockets.data(), _sockets.size(), -1);
		if (serverActivity > 0)
		{
			if (_sockets[0].revents & POLLIN)
			{
				handleNewConnectionRequest();
				serverActivity--;
			}
			for (unsigned int i = _sockets.size() - 1; i > 0 && serverActivity > 0; --i)
			{
				if (_sockets[i].fd == _quoteBot->getApiSocketFd())
				{
					if (handleApiEvent(_sockets[i]) == true)
					{
						serverActivity--;
						continue;
					}
				}
				if (_sockets[i].revents & (POLLHUP | POLLERR | POLLNVAL))
				{
					disconnectClient(_clients[_sockets[i].fd]);
					serverActivity--;
				}
				else if (_sockets[i].revents & POLLIN)
				{
					msg.receiveMessage(*(_clients[_sockets[i].fd]));
					serverActivity--;
				}
			}
		}
	}
}


// ************************************************************************** //
//                             Static Variables                               //
// ************************************************************************** //

Server*	Server::instance = NULL;
bool Server::_running = true;
