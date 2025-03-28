#include "../include/Utils.hpp"
#include "../include/Channel.hpp"

// ************************************************************************** //
//                       Constructors & Desctructors                          //
// ************************************************************************** //
Channel::Channel(std::string name) : _channelName(name) {
    printStr("Channel default created! :D", PURPLE);
}

Channel::Channel(const Channel & other) {  
    *this = other;
    printStr("Channel copied (deep copy unnecessary)! :D", PURPLE);
}

Channel::~Channel(void){
    printStr("Channel destroyed! D:", PURPLE);
}

// ************************************************************************** //
//                           Operator Overloads                               //
// ************************************************************************** //
Channel & Channel::operator=(const Channel &other) {
    (void)other;
    return *this;
}

// ************************************************************************** //
//                               Accessors                                    //
// ************************************************************************** //
std::string       Channel::getName(void) const {
    return _channelName;
}
// ************************************************************************** //
//                             Public Functions                               //
// ************************************************************************** //

// ************************************************************************** //
//                             Private Functions                              //
// ************************************************************************** //

// ************************************************************************** //
//                            Non-member Functions                            //
// ************************************************************************** //