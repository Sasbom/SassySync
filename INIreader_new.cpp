#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>
#include <fstream>
#include <chrono>
#include <future>
#include <thread>
#include <ctime>
#include <stdexcept>
#include <vector>
#include <windows.h>
#include <optional>
#include <array>
#include <bits/stl_pair.h>
#include <unordered_map>


void rem_trail_lead_spaces(std::string_view& string)
{
    size_t pos_start = 0;
    size_t pos_end   = string.length()-1;

    while(string.at(pos_start)==' ')
    {
        pos_start++;
    }

    while(string.at(pos_end)==' ')
    {
        pos_end--;
    }

    string = string.substr(pos_start,pos_end-pos_start+1);
}

std::string normalize_path(std::string_view& path) 
{
    rem_trail_lead_spaces(path);
    
    std::string spath = std::string(path);
    size_t pos = spath.find('\\');
    while(pos!=std::string::npos)
    {
        spath.replace(pos,1,"/");
        pos = spath.find('\\');
    }
    return spath;
}

std::optional<bool> bool_from_string(std::string_view const& boolstring)
{
    std::array checkTrue = std::to_array((std::string_view[]){"1","true","True","TRUE"});
    std::array checkFalse = std::to_array((std::string_view[]){"0","false","False","FALSE"});
    
    for(auto const& item : checkTrue)
    {
        if(boolstring.find(item)!=std::string_view::npos) return true;
    }
    
    for(auto const& item : checkFalse)
    {
        if(boolstring.find(item)!=std::string_view::npos) return false;
    }

    return {};
}

std::optional<std::pair<std::string_view,std::string_view>> parse_line(std::string_view const& line)
{
    if(line.find('[',0)==std::string_view::npos || line.find(']',0)==std::string_view::npos) return {} ;
    
    size_t open_bracket = line.find_first_of('[',0);
    size_t close_bracket = line.find_first_of(']',0);
    std::string_view key = line.substr(open_bracket+1,close_bracket-open_bracket-1);
    rem_trail_lead_spaces(key);

    size_t colon = line.find(':',0);

    if(colon==std::string_view::npos) return std::make_pair(key,"");

    std::string_view value = line.substr(colon+1);
    rem_trail_lead_spaces(value);

    return std::make_pair(key,value);
};

enum datatype
{
    none,
    boolean_t,
    number,
    string,
    path,
};

struct INIstruct
{
std::filesystem::path folder_in; //folder to get files FROM
std::filesystem::path folder_to; //DESTINATION of files
std::chrono::minutes  wait_time;
std::string_view      name = "service";

bool setup_ok = false;
bool del_to   = false;
bool run      = false;

bool safety   = true;

static constexpr std::string_view safety_path = "X:/"; //hardcoded safety path.

static std::unordered_map<std::string,enum datatype> const setting_type;

INIstruct() = delete;

INIstruct(std::fstream& file, std::string_view const& _name) : 
    name(_name) 
{
    std::string line;

    while(getline(file,line))
    {
        if(line.find("#")!=std::string::npos || line.length() == 0) continue; //comment or empty line
        
        auto const parse = parse_line(line);
        
        if(!parse.has_value()) continue; //nothing found, we keep goin.
        
        auto const& key = parse.value().first;
        auto const& value = parse.value().second;
    }

};

};

std::unordered_map<std::string,enum datatype> const INIstruct::setting_type = {
                                                                            {"SERVICE",string},
                                                                            {"FROM FOLDER",path},
                                                                            {"DESTINATION FOLDER",path},
                                                                            {"RUN",boolean_t},
                                                                            {"DELAY",number},
                                                                            {"DELETE NON MATCHING",boolean_t},
                                                                            {"SAFETY",boolean_t},
                                                                            {"SERVICE END",none}
                                                                        };


int main()
{
    auto p = parse_line("[ potjandorie                    ] :                50");

    std::cout << p.value().first << " : " << p.value().second;
    
    
    return 0;
}