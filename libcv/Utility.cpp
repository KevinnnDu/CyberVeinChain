

#include "Utility.h"

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <libdevcore/SHA3.h>
#include <libdevcore/RLP.h>
#include <libdevcore/easylog.h>
#include <libcvcore/Common.h>
#include "BlockChain.h"
#include "Defaults.h"
using namespace std;
using namespace dev;
using namespace dev::eth;
namespace fs = boost::filesystem;

bytes dev::eth::parseData(string const& _args)
{
	bytes m_data;

	boost::smatch what;
	static const boost::regex r("(!|#|@|\\$)?\"([^\"]*)\"(\\s.*)?");
	static const boost::regex d("(@|\\$)?([0-9]+)(\\s*(ether)|(finney)|(szabo))?(\\s.*)?");
	static const boost::regex h("(@|\\$)?(0x)?(([a-fA-F0-9])+)(\\s.*)?");

	string s = _args;
	while (s.size())
		if (boost::regex_match(s, what, d))
		{
			u256 v((string)what[2]);
			if (what[6] == "szabo")
				v *= dev::eth::szabo;
			else if (what[5] == "finney")
				v *= dev::eth::finney;
			else if (what[4] == "ether")
				v *= dev::eth::ether;
			bytes bs = dev::toCompactBigEndian(v);
			if (what[1] != "$")
				for (auto i = bs.size(); i < 32; ++i)
					m_data.push_back(0);
			for (auto b: bs)
				m_data.push_back(b);
			s = what[7];
		}
		else if (boost::regex_match(s, what, h))
		{
			bytes bs = fromHex(((what[3].length() & 1) ? "0" : "") + what[3]);
			if (what[1] != "$")
				for (auto i = bs.size(); i < 32; ++i)
					m_data.push_back(0);
			for (auto b: bs)
				m_data.push_back(b);
			s = what[5];
		}
		else if (boost::regex_match(s, what, r))
		{
			bytes d = asBytes(what[2]);
			if (what[1] == "!")
				m_data += FixedHash<4>(sha3(d)).asBytes();
			else if (what[1] == "#")
				m_data += sha3(d).asBytes();
			else if (what[1] == "$")
				m_data += d + bytes{0};
			else
				m_data += d + bytes(32 - what[2].length() % 32, 0);
			s = what[3];
		}
		else
			s = s.substr(1);

	return m_data;
}

void dev::eth::upgradeDatabase(std::string const& _basePath, h256 const& _genesisHash)
{
	std::string path = _basePath.empty() ? Defaults::get()->dbPath() : _basePath;

	//判断是否有该status和details和blocks文件  在path下有该对应文件 则复制拷贝到 对应的chainpath和extrasPath下面去
	if (fs::exists(path + "/state") && fs::exists(path + "/details") && fs::exists(path + "/blocks"))
	{
		// upgrade
		LOG(INFO) << "Upgrading database to new layout...";
		bytes statusBytes = contents(path + "/status");
		RLP status(statusBytes);
		try
		{
			auto minorProtocolVersion = (unsigned)status[1];
			auto databaseVersion = (unsigned)status[2];
			auto genesisHash = status.itemCount() > 3 ? (h256)status[3] : _genesisHash;

			string chainPath = path + "/" + toHex(genesisHash.ref().cropped(0, 4));
			string extrasPath = chainPath + "/" + toString(databaseVersion);

			// write status
			if (!fs::exists(chainPath + "/blocks"))
			{
				fs::create_directories(chainPath);
				DEV_IGNORE_EXCEPTIONS(fs::permissions(chainPath, fs::owner_all));
				fs::rename(path + "/blocks", chainPath + "/blocks");

				if (!fs::exists(extrasPath + "/extras"))
				{
					fs::create_directories(extrasPath);
					DEV_IGNORE_EXCEPTIONS(fs::permissions(extrasPath, fs::owner_all));
					fs::rename(path + "/details", extrasPath + "/extras");
					fs::rename(path + "/state", extrasPath + "/state");
					writeFile(extrasPath + "/minor", rlp(minorProtocolVersion));
				}
			}
		}
		catch (Exception& ex)
		{
			LOG(WARNING) << "Couldn't upgrade: " << ex.what() << boost::diagnostic_information(ex);
		}
		catch (...)
		{
			LOG(WARNING) << "Couldn't upgrade. Some issue with moving files around. Probably easiest just to redownload.";
		}

		fs::rename(path + "/status", path + "/status.old");
	}
}

