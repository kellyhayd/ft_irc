#include "File.hpp"

File::File() : state(NONE), senderClient(NULL), recipientClient(NULL), lastActivityTime(0) {}

File::~File()
{
    if (tempFileStream.is_open())
        tempFileStream.close();
    info("File destroyed for ID: " + transferId);
}
    
// File::File()
// {
// 	_fileName = "";
// 	_filePath = "";
// }

// File::File(std::string fileName, std::string filePath, std::string sender, std::string receiver) :
// 	_fileName(fileName), _filePath(filePath), _sender(sender), _receiver(receiver) {}

// File::~File() {}

// std::string	File::getFileName(void) const { return _fileName; };

// std::string	File::getFilePath(void) const { return _filePath; };

// std::string	File::getSender(void) const { return _sender; };

// std::string	File::getReceiver(void) const { return _receiver; };

// void	File::setFileName(std::string fileName) { _fileName = fileName; };

// void	File::setFilePath(std::string filePath) { _filePath = filePath; };

// void	File::setSender(std::string sender) { _sender = sender; };

// void	File::setReceiver(std::string receiver) { _receiver = receiver; };

// bool	File::compare(Client& receiver, std::string sender, std::string fileName) const
// {
// 	if (receiver.nickname() == _receiver && sender == _sender && fileName == _fileName)
// 		return true;
// 	return false;
// }
