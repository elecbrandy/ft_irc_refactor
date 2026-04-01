#include "Cmd.hpp"

/*
    Author : sejkim2
    Description : /MODE
    /mode [flag][options] [params]
    
    flag : [+/-]
    option : [t/o/l/i/k]
    params : [o : nickname / l : size / k : password]
*/

void printParam(std::vector<std::string> param)
{
    std::cout << "this is start" << '\n';
    for(std::vector<std::string>::iterator it = param.begin(); it != param.end(); it++)
        std::cout << *it << "\n";
    std::cout << "this is end" << '\n';
}

// option +-o
void Cmd::validationNickName(std::string nickname, Channel* channel, int plus_flag)
{
    //해당 클라이언트가 채널에 없음
    if (plus_flag == 1)
    {
        if (channel->isParticipant(channel->isOperatorNickname(nickname)) == false)
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_USERNOTINCHANNEL(this->client->getNickname(), nickname, channel->getName())));
    }
    else
    // plus_flag == 0, 채널의 운영자에 nickname이 포함되어 있는지 검증
    {
        if (channel->isOperator(nickname) == false)
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOSUCHNICK(this->client->getNickname(), nickname)));
    }
}

//option : +k (채널에 password 설정)
void Cmd::validationKey(std::string key, Channel* channel, int plus_flag)
{
    if (plus_flag == 1)
    {
        if (channel->isSetKey() == true)
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_KEYSET(channel->getName())));

        if (key.empty())
        {
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(client->getNickname(), "PASS")));
        }

        /* SIZE check */
        if (key.size() >= PASSWORD_MAX_LEN)
        {
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_PASS_PASSWORD));
        }
    }
    else
    {
        if (key.compare(channel->getKey()) != 0)
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_KEYSET(channel->getName())));
    }
}

//option : +l (채널 제한 인원 설정)
void Cmd::validationInt(std::string _size)
{
    size_t _len = _size.length();

    if (_size[0] == '0' && _len > 1) 
        throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_INVALIDPARAM));
    
    for (size_t i = 0; i < _len; ++i) {
        if (!std::isdigit(_size[i])) {
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_INVALIDPARAM));
        }
    }
}

void Cmd::addChannelOperator(std::string nickname, Channel* channel)
{
    std::map<std::string, Client*> map = this->server.getNickNameClientMap();
    std::map<std::string, Client*>::iterator client = map.find(nickname);
    
    //client->option_o (flag = 1)
    std::string nick = "@" + nickname;
    channel->removeParticipant(nickname);
    channel->addParticipant(nick, client->second);

    channel->setMode('o');
    channel->addOperator(nickname, client->second);
}

// #ch -o user
void Cmd::removeChannelOperator(std::string nickname, Channel* channel)
{
    std::map<std::string, Client*> map = this->server.getNickNameClientMap();
    std::map<std::string, Client*>::iterator client = map.find(nickname);
    
    //client->option_o (flag = 0)
    std::string nick = "@" + nickname;
    channel->removeParticipant(nick);
    channel->addParticipant(nickname, client->second);

    channel->removeMode('o');
    channel->removeOperator(nickname);
}

void Cmd::setChannelKey(std::string key, Channel* channel)
{
    channel->setMode('k');
    channel->setKey(key);
}

void Cmd::removeChannelKey(Channel* channel)
{
    channel->removeMode('k');
    channel->removeKey();
}

void Cmd::setChannelUserLimit(std::string _size, Channel* channel)
{
    channel->setMode('l');
    channel->setLimit(atoi(_size.c_str()));
}

void Cmd::removeChannelUserLimit(Channel* channel)
{
    channel->removeMode('l');
    channel->setLimit(DEFAULT_LIMIT);
}

void Cmd::handlePlusFlagOption(std::vector<std::string> modeParse, std::map<std::string, Channel*>::iterator channel, int plus_flag)
{
    //{i, t, k, o, l} 인가
        std::string _option = modeParse[1].substr(1);
        std::vector<char> use_param_mode;
        int flag_i = 0;
        int flag_t = 0;
        for (size_t i = 0; i < _option.length(); i++)
        {
            if (_option[i] == 'o' || _option[i] == 'k' || _option[i] == 'l')
                use_param_mode.push_back(_option[i]);
            else if (_option[i] == 'i')
                flag_i = 1;
            else if (_option[i] == 't')
                flag_t = 1;
            else
                throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_UNKNOWNMODE(std::string(1, _option[i]))));
        }
        //o, k, l은 혼합하여 3개까지 가능
        if (use_param_mode.size() > 3)
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(this->client->getNickname(), this->cmdParams)));

        // k, o, l의 개수와 파라미터 개수가 동일한가
        // 3개를 넘지 않는가
        std::vector<std::string> param(modeParse.begin() + 2, modeParse.end());
        if (param.size() != use_param_mode.size() || param.size() > 3)
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(this->client->getNickname(), this->cmdParams)));

        // param validation
        size_t option_index = 0;
        size_t param_index = 0;
        size_t _size = use_param_mode.size();
        while (option_index < _size)
        {
            if (use_param_mode[option_index] == 'o')
                validationNickName(param[param_index], channel->second, plus_flag);
            else if (use_param_mode[option_index] == 'k')
                validationKey(param[param_index], channel->second, plus_flag);
            //(_option[option_index] == 'l')
            else
                validationInt(param[param_index]);
            option_index++;
            param_index++;
        }

        // executeOption
        if (flag_i == 1)
            channel->second->setMode('i');
        if (flag_t == 1)
            channel->second->setMode('t');

        option_index = 0;
        param_index = 0;
        while (option_index < _size)
        {
            if (use_param_mode[option_index] == 'o')
                addChannelOperator(param[param_index], channel->second);
            else if (use_param_mode[option_index] == 'k')
                setChannelKey(param[param_index], channel->second);
            //(_option[option_index] == 'l')
            else
                setChannelUserLimit(param[param_index], channel->second);
            option_index++;
            param_index++;
        }
}

void Cmd::handleMinusFlagOption(std::vector<std::string> modeParse, std::map<std::string, Channel*>::iterator channel, int plus_flag)
{
    //{i, t, k, o, l} 인가
        std::string _option = modeParse[1].substr(1);
        std::vector<char> use_param_mode;
        int flag_i = 0;
        int flag_t = 0;
        int flag_l = 0;
        int flag_k = 0;
        for (size_t i = 0; i < _option.length(); i++)
        {
            if (_option[i] == 'o')
                use_param_mode.push_back(_option[i]);
            else if (_option[i] == 'i')
                flag_i = 1;
            else if (_option[i] == 't')
                flag_t = 1;
            else if (_option[i] == 'l')
                flag_l = 1;
            else if (_option[i] == 'k')
            {
                use_param_mode.push_back(_option[i]);
                flag_k = 1;
            }
            else
                throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_UNKNOWNMODE(std::string(1, _option[i]))));
        }
        if (use_param_mode.size() > 3)
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(this->client->getNickname(), this->cmdParams)));

        // o의 개수와 파라미터 개수가 동일한가
        // 3개를 넘지 않는가
        std::vector<std::string> param(modeParse.begin() + 2, modeParse.end());
        if (param.size() != use_param_mode.size() || param.size() > 3)
            throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NEEDMOREPARAMS(this->client->getNickname(), this->cmdParams)));

        // param validation
        size_t option_index = 0;
        size_t param_index = 0;
        size_t _size = use_param_mode.size();
        while (option_index < _size)
        {
            if (use_param_mode[option_index] == 'o')
                validationNickName(param[param_index], channel->second, plus_flag);
            else
                validationKey(param[param_index], channel->second, plus_flag);
            option_index++;
            param_index++;
        }

        // executeOption
        if (flag_i == 1)
            channel->second->removeMode('i');
        if (flag_t == 1)
            channel->second->removeMode('t');
        if (flag_l == 1)
            channel->second->removeMode('l');
        if (flag_k == 1)
            removeChannelKey(channel->second);

        option_index = 0;
        param_index = 0;
        while (option_index < _size)
        {
            if (use_param_mode[option_index] == 'o')
                removeChannelOperator(param[param_index], channel->second);
            option_index++;
            param_index++;
        }
}

void Cmd::cmdMode()
{
    std::vector<std::string> modeParse;
    std::istringstream stream(this->cmdParams);
    std::string token;
    std::map<std::string, Channel*> _channels = this->server.getChannels();
    int plus_flag = 0;
    
    while (stream >> token)
        modeParse.push_back(token);

    std::string channelName = modeParse[0];

    //servername validation
    std::map<std::string, Channel*>::iterator channel = _channels.find(channelName);

    //존재하는 채널인가
    if (channel == _channels.end())
        throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOSUCHCHANNEL(this->client->getNickname(), modeParse[0])));
    //호출자가 현재 채널에 참여하고 있는가
    std::map<std::string, Client*> participaciant = channel->second->getParticipant();
    if (participaciant.find(channel->second->isOperatorNickname(this->client->getNickname())) == participaciant.end())
        throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_NOTONCHANNEL(this->client->getNickname(), channelName)));

    // /mode #ch -> #ch의 옵션 정보 출력 (구현 범위 x)
    if (modeParse.size() == 1)
        return;

    //채널의 운영자가 현재 호출 클라이언트인가
    if ((channel->second)->isOperator(this->client->getNickname()) == false)
        throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_CHANOPRIVSNEEDED(this->client->getNickname(), modeParse[0])));

    // option validation // 옵션이 +나 -로 시작하는가
    if (modeParse[1].at(0) == '+')
        plus_flag = 1;
    else if (modeParse[1].at(0) == '-')
        plus_flag = 0;
    else
        throw CmdException(server.makeMsg(PREFIX_SERVER, ERR_UNKNOWNMODE(std::string(1, modeParse[1].at(0)))));

    //excute
    if (plus_flag == 1)
        handlePlusFlagOption(modeParse, channel, plus_flag);
    else
        handleMinusFlagOption(modeParse, channel, plus_flag);

    std::string str = "MODE";
    size_t i = 0;
    for(i = 0; i < modeParse.size() - 1; i++)
        str += " " + modeParse[i];
    str += " :" + modeParse[i];
    std::string msg = server.makeMsg(this->client->getPrefix(), str);
    
    server.broadcastMsg(msg, channel->second, -1);
}
