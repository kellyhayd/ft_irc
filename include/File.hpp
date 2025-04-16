#ifndef FILE_HPP
#define FILE_HPP

#include "irc.hpp"

class Client;

enum TransferState
{
	NONE,
	PENDING_UPLOAD,
	UPLOADING,
	UPLOAD_COMPLETE,
	PENDING_DOWNLOAD,
	DOWNLOADING,
	TRANSFER_COMPLETE,
	FAILED,
	CANCELLED
};

class File
{
public:
	std::string transferId;
	std::string senderNick;
	std::string recipientNick;
	std::string fileName;
	// size_t          expectedSize;
	// size_t          receivedSize;
	std::string tempFilePath;
	std::fstream tempFileStream;
	TransferState state;
	Client *senderClient;
	Client *recipientClient;
	time_t lastActivityTime;

	File();
	~File();
};


// class File
// {
// 	private:
// 		std::string	_fileName;
// 		std::string	_filePath;
// 		std::string	_sender;
// 		std::string	_receiver;

// 	public:
// 		File();
// 		File(std::string fileName, std::string filePath, std::string sender, std::string receiver);
// 		~File();

// 		std::string	getFileName(void) const;
// 		std::string	getFilePath(void) const;
// 		std::string	getSender(void) const;
// 		std::string	getReceiver(void) const;

// 		void	setFileName(std::string fileName);
// 		void	setFilePath(std::string filePath);
// 		void	setSender(std::string sender);
// 		void	setReceiver(std::string receiver);

// 		bool	compare(Client& receiver, std::string sender, std::string fileName) const;
// };

#endif