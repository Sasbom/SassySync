// Cleaner rewrite of INI parser with less code and easier expandability
// Sas van Gulik, 6-9-2022

#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <stdexcept>
#include <vector>
#include <windows.h>
#include <optional>
#include <array>
#include <bits/stl_pair.h>
#include <unordered_map>
#include "headers/sas_threadpool.hpp"

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
    std::array checkTrue = 
        std::to_array((std::string_view[]){"1","true","True","TRUE"});

    std::array checkFalse = 
        std::to_array((std::string_view[]){"0","false","False","FALSE"});
    
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
    std::string_view key = 
        line.substr(open_bracket+1,close_bracket-open_bracket-1);
    
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

//Struct to parse and gather all services
struct SettingsList;

//data to pass to service.
struct INIstruct
{
    std::filesystem::path folder_in; //folder to get files FROM
    std::filesystem::path folder_to; //DESTINATION of files
    std::chrono::seconds  wait_time = std::chrono::seconds(600);
    std::string           name;

    INIstruct(std::string_view const& _name) : name(_name){};

    //buncha checks
    bool setup_ok = false;
    bool del_to   = false;
    bool run      = true;

    bool found_from = false;
    bool found_to = false;
    bool found_timer = false;

    bool safety   = true;

    static constexpr std::string_view safety_path = "Y:/"; 
};

bool verify_INI(INIstruct& I)
{
    if(!I.found_from || !I.found_to)
    {
        std::cout << "Service: " << I.name << " didn't initialize properly.\n";
        if(!I.found_from) printf("[FROM FOLDER] is missing, doesn't exist, or is not recognised in settings.ini\n");
        if(!I.found_to) printf("[DESTINATION FOLDER] is missing, doesn't exist, or is not recognised in settings.ini\n");
        if(!I.found_timer) printf("[DELAY] is missing or not recognised in settings.ini\n");
        printf("Running this service will be halted.\n\n");

    }
    else
    {
        printf("Service: %s\n==--==--==--==--==--==--==--==--==--==--==--==--\n",std::string(I.name).c_str());
        printf("Source folder: %s\n",I.folder_in.string().c_str());
        printf("Destination folder: %s\n",I.folder_to.string().c_str());
        printf("Running once every %f minutes\n",float(I.wait_time.count())/60);
        if(I.del_to) printf("Delete non-matching is turned on. \nFiles in the destination folder will be deleted if they don't match the source.\n");
        if(I.safety && I.folder_in.string().find(I.safety_path)==0)
        { 
            printf("Drive Safety is on! Hard syncing with destination in %s will be disabled.\n",std::string(I.safety_path).c_str());
            I.del_to = false;
        }
        
        if(I.run) 
            printf("Service: %s is set up and is scheduled to run.\n\n",std::string(I.name).c_str());
        else
            printf("Service: %s is set up and is scheduled to NOT run.\n\n",std::string(I.name).c_str());
        
        I.setup_ok = true;
    }
    return (I.run&&I.setup_ok);
}

//the function signature is a pointer to the settings, and the gathered value.
using func_t = std::function<void(SettingsList*, std::string_view const&)>;

using func_map_t = std::unordered_map<std::string_view,func_t>;

struct SettingsList
{
    static func_map_t const parse_functions;

    INIstruct* INI_cur = nullptr;
    std::vector<INIstruct> services = {};

    bool run = true;
    bool hide = false;

    SettingsList()
    {
        std::fstream file("settings.ini");
        std::string line;
        while(getline(file,line))
        {
            if(line.find("#")!=std::string_view::npos || line.length() == 0) continue; //comment or empty line

            auto const parse = parse_line(line);
        
            if(!parse.has_value()) continue; //nothing found, continue;

            auto const& key = parse.value().first;
            auto const& value = parse.value().second;

            //run key through map and run function stored within.
            try
            {
                auto func = parse_functions.at(key);
                func(this, value);
            }
            catch(std::out_of_range const& except)
            {
                std::cout<< "keyword "<< key << " not present\n";
            }
            catch(std::runtime_error const& except)
            {
                std::cout << except.what();
                std::cout << "Quitting...\n";
                run = false;
                break;
            }

        }
        //don't run if there's nothing to run.
        if(services.empty()) run = false;
    };
};

//set up map
func_map_t const SettingsList::parse_functions = 
{
    {"SERVICE", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            if(Settings->INI_cur!=nullptr)
            {
                throw std::runtime_error("[ERROR] : SERVICE not properly initialised.\n");
            }
            INIstruct* new_ini = new INIstruct(val);
            Settings->INI_cur = new_ini;
        }
    },
    {"FROM FOLDER", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto INI = Settings->INI_cur;
            auto path_value = normalize_path(val);
            INI->folder_in = std::filesystem::path(path_value);
            INI->found_from = std::filesystem::exists(INI->folder_in);
        }
    },
    {"DESTINATION FOLDER", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto INI = Settings->INI_cur;
            auto path_value = normalize_path(val);
            INI->folder_to = std::filesystem::path(path_value);
            INI->found_to = std::filesystem::exists(INI->folder_to);
        }
    },
    {"DELAY", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto INI = Settings->INI_cur;
            int val_i = int(string_to_float(std::string(val)))*60;
            if(val_i!=-1 && val_i > 0)
            {
                INI->wait_time = std::chrono::seconds(val_i);
                INI->found_timer = true;
            }
            else
            {
                INI->wait_time = std::chrono::seconds(600);
                INI->found_timer = true;
            }
        }
    },
    {"DELAY S", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto INI = Settings->INI_cur;
            int val_i = int(string_to_float(std::string(val)));
            if(val_i!=-1 && val_i > 0)
            {
                INI->wait_time = std::chrono::seconds(val_i);
                INI->found_timer = true;
            }
            else
            {
                INI->wait_time = std::chrono::seconds(600);
                INI->found_timer = true;
            }
        }
    },
    {"DELETE NON-MATCHING", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto INI = Settings->INI_cur;
            auto opt_bool = bool_from_string(val);
            (opt_bool.has_value())? 
                INI->del_to = opt_bool.value() : INI->del_to = false;
        }
    },
    {"SAFETY", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto INI = Settings->INI_cur;
            auto opt_bool = bool_from_string(val);
            (opt_bool.has_value())? 
                INI->safety = opt_bool.value() : INI->safety = true;
        }
    }, 
    {"RUN", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto INI = Settings->INI_cur;
            auto opt_bool = bool_from_string(val);
            (opt_bool.has_value())? 
                INI->run = opt_bool.value() : INI->run = false;
        }
    },
    {"GLOBAL RUN", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto opt_bool = bool_from_string(val);
            (opt_bool.has_value())? 
                Settings->run = opt_bool.value() : Settings->run = true;
        }
    },
    {"GLOBAL HIDE", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            auto opt_bool = bool_from_string(val);
            (opt_bool.has_value())? 
                Settings->hide = opt_bool.value() : Settings->hide = false;
        }
    },
    {"SERVICE END", 
        [](SettingsList* Settings, std::string_view const& val)
        {       
            if(Settings->INI_cur==nullptr)
            {
                throw std::runtime_error("[ERROR] : SERVICE END not properly initialised.\n");
            }

            auto INI = *Settings->INI_cur;
            std::cout << "Verifying: " << INI.name << "...\n\n";
            if(verify_INI(INI))
            {
                Settings->services.push_back(INI);
                Settings->INI_cur = nullptr;
            }
            else
            {
            //free memory if setting is not going to run.
            delete Settings->INI_cur;
            Settings->INI_cur = nullptr;
            std::cout << "Deleted service from memory. \n\n";
            }
        }
    },
};

//SERVICE COMPONENT

//run job according to settings stored in INIstruct
void job_sync(INIstruct const& settings)
{
    //setting some options for updating files when nessecary

    std::filesystem::copy_options cpy_options = std::filesystem::copy_options::overwrite_existing;

    for(std::filesystem::directory_entry dir 
        : std::filesystem::recursive_directory_iterator(settings.folder_in))
    {
        std::string cur_path = dir.path().string();
        normalize_path(cur_path);

        //figure out equivaluent path on the other side.
        std::string rel_to_path = settings.folder_to.string()
                                  + cur_path.substr(settings.folder_in.string().length());

        //if it doesn't exist, let's make it.
        if(!std::filesystem::exists(rel_to_path))
        {
            if(std::filesystem::is_directory(cur_path))
            {
                std::filesystem::create_directory(rel_to_path);
            }
            else if (std::filesystem::is_regular_file(cur_path))
            {
                std::filesystem::copy(cur_path,rel_to_path,cpy_options);
            }
        }
        else
        {
            if (std::filesystem::is_regular_file(cur_path))
            {
                auto time_in_file = std::filesystem::last_write_time(cur_path);
                auto time_to_file = std::filesystem::last_write_time(rel_to_path);
                
                if(time_in_file>time_to_file)
                {
                    std::filesystem::remove(rel_to_path); // safeguard
                    try {
                        std::filesystem::copy(cur_path,rel_to_path,cpy_options);
                    } catch (std::exception& e) {
                        std::cout << "Exception occurred during update of file " << rel_to_path <<":\n" << e.what() << "\n\nMoving on...";
                    }
                }
            }
        }
    }

    if(settings.del_to)
    {
    for(std::filesystem::directory_entry dir 
        : std::filesystem::recursive_directory_iterator(settings.folder_to))
    {
        std::string to_path = dir.path().string();
        normalize_path(to_path);

        std::string rel_in_path = settings.folder_in.string()
                    + to_path.substr(settings.folder_to.string().length());
    
        if(!std::filesystem::exists(rel_in_path))
        {
            std::filesystem::remove_all(to_path);
        }
    }
    }
}

void service_routine(INIstruct const& settings,std::mutex& mut)
{
    while(true)
    {
        auto now = std::chrono::system_clock::now();
        auto nowt = std::chrono::system_clock::to_time_t(now);
        auto time = std::chrono::high_resolution_clock::now();
        
        {
        std::lock_guard<std::mutex> lk(mut);
        
        //print start
        now = std::chrono::system_clock::now();
        nowt = std::chrono::system_clock::to_time_t(now);
        std::cout << "[SERVICE JOB START] - Sync service: "<< settings.name <<" started at: " << std::ctime(&nowt) << "\n";
        time = std::chrono::high_resolution_clock::now();
        }

        //do work
        job_sync(settings);

        {
        std::lock_guard<std::mutex> lk(mut);
        
        //define variables.
        auto time2 = std::chrono::high_resolution_clock::now();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time2-time);

        //print end
        auto now2 = std::chrono::system_clock::now();
        auto nowt2 = std::chrono::system_clock::to_time_t(now2);
        std::cout << "[SERVICE JOB END] - Sync service " << settings.name << " ended at: " << std::ctime(&nowt2) 
        << "---------------------------------------------------------------------------------\n"
        << "- Service " << settings.name <<" took: " << seconds.count() <<" seconds\n"
        << "- Waiting " << (float(settings.wait_time.count())/60) << " minutes for next run of " << settings.name<<".\n\n";
        }


        std::this_thread::sleep_for(settings.wait_time);
    }
}

int main()
{
auto Settings = SettingsList();
if(!Settings.run) return 0;

if(Settings.hide)
{
    HWND hWnd = GetConsoleWindow();
    ShowWindow( hWnd, SW_HIDE );
}

std::mutex service_mutex;

//create a pool of needed size.
threadPool pool(Settings.services.size());
pool.Run();

std::cout << "SassySync v0.6 made by Sas van Gulik.\n\n";

//setup services to run.
for (auto const& ini : Settings.services)
{
    pool.addJob_bind(service_routine,std::ref(ini),std::ref(service_mutex));
}

pool.waitForAll();

return 0;
}