#ifndef SERVER_HPP
#define SERVER_HPP
#include "irc.hpp"
#include "QuoteBot.hpp"
#include "File.hpp"

class	Client;
class	QuoteBot;
class	File;

typedef std::pair<int, Client *>	client_pair_t;
typedef std::map<int, Client *>		clients_t;
typedef std::map<std::string, File *> transfersMap;


class Server
{
	protected:

	private:
		std::vector<pollfd>		_sockets;
		clients_t				_clients;
		std::string				_password;
		unsigned int			_port;
		static bool				_running;
		QuoteBot*				_quoteBot;
		File*					_file;

		std::map<std::string, std::string>	_opers;
		transfersMap	_activeTransfers;

		pollfd	_makePollfd(int fd, short int events, short int revents);

	public:
		/* construcotrs & destructors */
		Server(int port_num, std::string &passwd);
		~Server(void);
		
		/* accessors*/
		std::map<std::string,std::string> 	getOpers(void);
		std::string							getPassword(void);
		unsigned int						getPort(void);
		clients_t&							getClients(void);
		Client*								getClientByUser(std::string& user) const;
		Client*								getClientByNick(std::string& nick) const;
		QuoteBot*							getQuoteBot(void);
		File*								getFile(void);
		
		/* member functions*/
		void 			run(void);
		void 			parseOpersConfigFile(const char *file);
		void 			handleNewConnectionRequest();
		void			validatePassword(std::string &password, Client &client);
		void			validateIRCOp(std::string &nickname, std::string &password, Client &client);
		void 			addclient(pollfd &clientSocket);
		void 			disconnectClient(Client *client);
		void			shutdown();
		void			addApiSocket(pollfd &api_pfd);
		void			removeApiSocket(int fd);
		void			setBot();
		bool			handleApiEvent(pollfd fd);
		void			setFile(File* file);
		void			sendFile(std::fstream &file, Client &client);
		File*			prepareUpload(Client* sender, const std::string& recipientNick, const std::string& filename);
		std::string		generateTempFilePath(const std::string& sender, const std::string& fileName);
		File*			getTransferById(const std::string& transferId);
		void			finalizeTransfer(const std::string& transferId, bool deleteTempFile);
		void 			checkTransferTimeouts();
		void			startSendingFile(const std::string& transferId);

		/* static members */
		static Server*  instance;
		static void 	SIGINTHandler(int signum);
	};

#endif
