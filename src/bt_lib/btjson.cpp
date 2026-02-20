#include "btjson.h"

#include <fstream>


json BT::json_load_from_disk(std::string const& fname)
{
    std::ifstream f{ fname };
    return json::parse(f);
}

void BT::json_save_to_disk(json const& data_j, std::string const& fname)
{
    std::ofstream f{ fname };
    f << data_j.dump(4);
}

