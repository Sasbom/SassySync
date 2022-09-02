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

std::string_view rem_trail_lead_spaces(std::string_view const& string)
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

    return string.substr(pos_start,pos_end-pos_start+1);
}

std::string normalize_path(std::string_view const& path) 
{
    std::string spath = std::string(rem_trail_lead_spaces(path));
    
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

float string_to_float(std::string const& float_str)
{
    try
    {
        return std::stof(float_str);
    }
    catch(std::invalid_argument const& except)
    {
        return -1;
    }
    catch(std::out_of_range const& except)
    {
        return -1;
    }
}

struct INIstruct
{
std::filesystem::path folder_in; //folder to get files FROM
std::filesystem::path folder_to; //DESTINATION of files
std::chrono::seconds  wait_time;
std::string_view      name = "[undefined service name]";

bool setup_ok = false;
bool del_to   = false;
bool run      = true;

bool safety   = true;

static constexpr std::string_view safety_path = "Y:/"; //hardcoded safety path.

INIstruct() = delete;

INIstruct(std::fstream& file, std::string_view const& _name) : 
    name(_name) 
{
    std::string line;
    //setup conditions;
    bool found_from = false;
    bool found_to = false;
    bool found_timer = false;

    while(getline(file,line))
    {
        if(line.find("#")!=std::string::npos || line.length() == 0) continue; //comment or empty line
        
        auto const parse = parse_line(line);
        
        if(!parse.has_value()) continue; //nothing found, we keep goin.
        
        auto const& key = parse.value().first;
        auto const& value = parse.value().second;

        if (key=="FROM FOLDER")
        {
            auto path_value = normalize_path(value);
            folder_in = std::filesystem::path(path_value);
            found_from = std::filesystem::exists(folder_in);
        }

        if (key=="DESTINATION FOLDER")
        {
            auto path_value = normalize_path(value);
            folder_to = std::filesystem::path(path_value);
            found_to = std::filesystem::exists(folder_in);
        }

        if (key=="DELAY")
        {
            int val_i = int(string_to_float(std::string(value)))*60;
            if(val_i!=-1 && val_i > 0)
            {
                wait_time = std::chrono::seconds(val_i);
                found_timer = true;
            }
            else
            {
                wait_time = std::chrono::seconds(600);
                found_timer = true;
            }
        }

        if (key=="DELETE NON-MATCHING")
        {
            auto opt_bool = bool_from_string(value);
            (opt_bool.has_value())? del_to = opt_bool.value() : del_to = false;
        }

        if (key=="SAFETY")
        {
            auto opt_bool = bool_from_string(value);
            (opt_bool.has_value())? safety = opt_bool.value() : safety = true;
        }

        if (key=="RUN")
        {
            auto opt_bool = bool_from_string(value);
            (opt_bool.has_value())? run = opt_bool.value() : run = false;
        }

        if (key=="SERVICE END") break;
        
    }

    //report and check if everything is ok.
    if(!found_from || !found_to)
    {
        printf("Service: %s didn't initialize properly.\n",std::string(name).c_str());
        if(!found_from) printf("[FROM FOLDER] is missing, doesn't exist, or is not recognised in settings.ini\n");
        if(!found_to) printf("[DESTINATION FOLDER] is missing, doesn't exist, or is not recognised in settings.ini\n");
        if(!found_timer) printf("[DELAY] is missing or not recognised in settings.ini\n");
        printf("Running this service will be halted.\n\n");

    }
    else
    {
        printf("Service: %s\n==--==--==--==--==--==--==--==--==--==--==--==--\n",std::string(name).c_str());
        printf("Source folder: %s\n",folder_in.string().c_str());
        printf("Destination folder: %s\n",folder_to.string().c_str());
        printf("Running once every %f minutes\n",float(wait_time.count())/60);
        if(del_to) printf("Delete non-matching is turned on. \nFiles in the destination folder will be deleted if they don't match the source.\n");
        if(safety && folder_in.string().find(safety_path)==0)
        { 
            printf("Drive Safety is on! Hard syncing with destination in %s will be disabled.\n",std::string(safety_path).c_str());
            del_to = false;
        }
        
        if(run) 
            printf("Service: %s is set up and is scheduled to run.\n\n",std::string(name).c_str());
        else
            printf("Service: %s is set up and is scheduled to NOT run.\n\n",std::string(name).c_str());
        
        setup_ok == true;
    }
    
};
};

struct SettingsList
{
    std::vector<INIstruct> services = {};
    bool run = true;

    SettingsList()
    {
        std::fstream file("settings.ini");
        std::string line;
        while(getline(file,line))
        {
            if(line.find("#")!=std::string::npos || line.length() == 0) continue; //comment or empty line

            auto const parse = parse_line(line);

            if(!parse.has_value()) continue; //nothing found, we keep goin.

            auto const& key = parse.value().first;
            auto const& value = parse.value().second;

            if (key=="SERVICE")
            {
                std::string_view name = rem_trail_lead_spaces(value);
                auto ini = INIstruct(file,name);
                if(ini.setup_ok && ini.run) services.push_back(ini);
            }

            if (key=="GLOBAL RUN")
            {
                auto opt_bool = bool_from_string(value);
                (opt_bool.has_value())? run = opt_bool.value() : run = true;
            }
        }
        
        if(!run) printf("[GLOBAL RUN] turned off! No service will be executed.\n");
        
        file.close();
    };
};

int main()
{
    auto Settings = SettingsList();
    return 0;
}