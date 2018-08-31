// Copyright (c) 2018- The Taucoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "clubleaderdb.h"

#include "leveldb/iterator.h"

#include <boost/lexical_cast.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread/exceptions.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/function.hpp>

using namespace std;
using boost::lexical_cast;

const std::string CClubLeaderDB::DB_PATH   = "/clubinfo/leader";
const std::string CClubLeaderDB::ADD_OP    = "A";
const std::string CClubLeaderDB::REMOVE_OP = "R";
const std::string CClubLeaderDB::DB_LEADER = "L";

CClubLeaderDB::CClubLeaderDB()
{
    options.create_if_missing = true;

    std::string db_path = GetDataDir(true).string() + std::string(CClubLeaderDB::DB_PATH);
    LogPrintf("Opening LevelDB in %s\n", db_path);

    leveldb::Status status = leveldb::DB::Open(options, db_path, &pdb);
    dbwrapper_private::HandleError(status);
    assert(status.ok());
    LogPrintf("Opened LevelDB successfully\n");
}

CClubLeaderDB::~CClubLeaderDB()
{
    delete pdb;
    pdb = NULL;
}

bool CClubLeaderDB::Write(std::string address, std::string height)
{
    leveldb::Status status = pdb->Put(leveldb::WriteOptions(), DB_LEADER + address, height);
    if(!status.ok())
    {
        LogPrintf("LevelDB write failure in clubleader module: %s\n", status.ToString());
        dbwrapper_private::HandleError(status);
        return false;
    }

    return true;
}

bool CClubLeaderDB::Delete(std::string address)
{
    leveldb::Status status = pdb->Delete(leveldb::WriteOptions(), DB_LEADER + address);
    if(!status.ok() && !status.IsNotFound())
    {
        LogPrintf("LevelDB write failure in clubleader module: %s\n", status.ToString());
        dbwrapper_private::HandleError(status);
        return false;
    }

    return true;

}

bool CClubLeaderDB::Commit()
{
    if (cache.size() == 0)
        return true;

    for(std::map<std::string, std::string>::const_iterator it = cache.begin();
        it != cache.end(); it++)
    {
        string key = it->first;
        vector<string> splitedStr;
        boost::split(splitedStr, key, boost::is_any_of("_"));
        if (splitedStr.size() != 2)
        {
            LogPrintf("%s, key string error:%s\n", __func__, key);
            continue;
        }
        string address = splitedStr[0];
        string height  = splitedStr[1];
        string op = it->second;
        if (op.compare(ADD_OP) == 0)
        {
            Write(address, height);
        }
        else if (op.compare(REMOVE_OP) == 0)
        {
            Delete(address);
        }
    }

    cache.clear();
    return true;

}

bool CClubLeaderDB::AddClubLeader(std::string address, int height)
{
    if (height < 0)
    {
        return false;
    }

    std::string key;
    std::string strHeight;
    try
    {
        strHeight = lexical_cast<std::string>(height);
    }
    catch(boost::bad_lexical_cast& e)
    {
        LogPrintf("type cast err %s %s\n", __func__, e.what());
        return false;
    }

    key = address + "_" + strHeight;

    std::map<std::string, std::string>::iterator it = cache.find(key);
    if (it != cache.end())
    {
        it->second = ADD_OP;
    }
    else
    {
        cache.insert(std::map<std::string, std::string>::value_type(key, ADD_OP));
    }

    return true;
}

bool CClubLeaderDB::RemoveClubLeader(std::string address, int height)
{
    if (height < 0)
    {
        return false;
    }

    std::string key;
    std::string strHeight;
    try
    {
        strHeight = lexical_cast<std::string>(height);
    }
    catch(boost::bad_lexical_cast& e)
    {
        LogPrintf("type cast err %s %s\n", __func__, e.what());
        return false;
    }

    key = address + "_" + strHeight;

    std::map<std::string, std::string>::iterator it = cache.find(key);
    if (it != cache.end())
    {
        it->second = REMOVE_OP;
    }
    else
    {
        cache.insert(std::map<std::string, std::string>::value_type(key, REMOVE_OP));
    }

    return true;
}

bool CClubLeaderDB::GetAllClubLeaders(std::vector<std::string>& leaders, int height)
{
    if (height < 0)
        return false;

    leaders.clear();

    boost::scoped_ptr<leveldb::Iterator> pcursor(pdb->NewIterator(leveldb::ReadOptions()));
    pcursor->SeekToFirst();

    while (pcursor->Valid())
    {
        boost::this_thread::interruption_point();

        try
        {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            std::string keyStr = ssKey.str();

            if (keyStr.length() > 2 && keyStr[0] == 'L')
            {
                std::string address = keyStr.substr(1, keyStr.length() - 1);

                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                std::string valueStr = ssValue.str();
                int h = lexical_cast<int>(valueStr);

                LogPrintf("%s height str:%s h:%d thresold:%d\n", __func__, valueStr, h, height);

                if (h <= height)
                {
                    leaders.push_back(address);
                }
            }
        }
        catch (const std::exception& e)
        {
            LogPrintf("%s %s\n", __func__, e.what());
            return false;
        }

        pcursor->Next();
    }

    return true;
}