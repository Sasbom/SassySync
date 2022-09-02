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

void remove_leadingWhiteSpace(std::string& string)
{
    while(string[0]==' ')
    {
        string = string.substr(1,string.length()-1);
    }
}

void remove_trailingWhiteSpace(std::string& string)
{
    while(string[string.length()-1]==' ')
    {
        string = string.substr(0,string.length()-2);
    }
}

//de-fuck-up paths
void normalizePath(std::string& path)
{
    //remove leading spaces
    while(path[0]==' ')
    {
        path = path.substr(1,path.length()-1);
    }
    
    //make nice C++ friendly forward slashes.
    size_t pos = path.find('\\');
    while(pos!=std::string::npos)
    {
        path.replace(pos,1,"/");
        pos = path.find('\\');
    }

    //remove end '/'
    if(path[path.length()-1]=='/')
    {
        path = path.substr(0,path.length()-1);
    }
}

//checks if a string has a nice boolean value
int checkStringBool(std::string& str)
{
    std::string checkTrue[4] = {"1","true","True","TRUE"};
    std::string checkFalse[4] = {"0","false","False","FALSE"};

    for(int i = 0; i < 4; i++)
    {
        if (str.find(checkTrue[i])!=std::string::npos)
        {
            return 1;
        }
    }

    for(int i = 0; i < 4; i++)
    {
        if (str.find(checkFalse[i])!=std::string::npos)
        {
            return 0;
        }
    }

    // -1 if invalid
    return -1;
}

struct INIstruct
{
std::filesystem::path folder_in; //folder to get files FROM
std::filesystem::path folder_to; //DESTINATION of files
std::chrono::seconds wait_time;
std::string name = "service";

int setup_ok = 0;

bool del_to = true;
bool run = true;

bool hku = 0;

INIstruct()
{
    std::fstream file("settings.ini");
    std::string line;
    bool found_from = false;
    bool found_to = false;
    bool found_timer = false;
    std::cout << "READING \"settings.ini\".\n\n";
    while(getline(file,line))
    {
        if(line.find("#")!=std::string::npos || line.length() == 0) continue; //spaces or empty lines won't be tolerated

        if(line.find("FROM")!=std::string::npos && !found_from)
        {
            size_t pos = line.find(":");
            std::string in = line.substr(pos+1);
            normalizePath(in);
            std::cout << in << " is found as [FROM FOLDER], "; 
            folder_in = std::filesystem::path(in);
            if(std::filesystem::exists(folder_in))
            {
                std::cout << "and it exists! \n\n";
                found_from = true;
            }
            else
            {
                std::cout << "and it does NOT exist! \n\n";
                found_from = false;
            }
        }

        if(line.find("DESTINATION")!=std::string::npos && !found_to)
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            normalizePath(out);
            std::cout << out << " is found as [DESTINATION FOLDER], "; 
            folder_to = std::filesystem::path(out);
            if(std::filesystem::exists(folder_to))
            {
                std::cout << "and it exists! \n\n";
                found_to = true;
            }
            else
            {
                std::cout << "and it does NOT exist! \n\n";
                found_to = false;
            }
        }

        if(line.find("DELAY")!=std::string::npos && !found_timer)
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            remove_leadingWhiteSpace(out);
            try
            {
                std::cout << "[DELAY] Timer found: " << stoi(out) << " seconds\n\n"; 
                wait_time = std::chrono::seconds(stoi(out));
                found_timer = true;
            }
            catch(std::invalid_argument const& except)
            {
                std::cout << "[DELAY] flag found, but value: \"" << out << "\" wasn't suitable for conversion, because is it not a number.\nSetting [DELAY] to: 30 seconds.\n\n";
                wait_time = std::chrono::seconds(30);
                found_timer = true;
            }
            catch(std::out_of_range const& except)
            {
                std::cout << "[DELAY] flag found, but value: \"" << out << "\" is out of range.\nSetting [DELAY] to: 30 seconds.\n\n";
                wait_time = std::chrono::seconds(30);
                found_timer = true;
            }
            
        }

        //Delete non matching 
        if(line.find("DELETE NON-MATCHING")!=std::string::npos) 
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            std::cout << "[DELETE NON MATCHING] found! :";
            if (checkStringBool(out)==1)
            {
                del_to = true;
                std::cout << " True! \n\nFiles at [DESTINATION FOLDER] will be deleted when they don't match [FROM FOLDER].\n\n";
            }
            else if (checkStringBool(out)==0)
            {
                del_to = false;
                std::cout << " False! \n\nFiles at [DESTINATION FOLDER] will !!NOT!! be deleted when they don't match [FROM FOLDER].\n\n";
            }
            else
            {
                std::cout << " [Non Boolean value]. Use something like: 1, 0, True, False, TRUE, FALSE, true, false. \n";
                std::cout << "Using Default behavior : True.\nFiles at [DESTINATION FOLDER] will be deleted when they don't match [FROM FOLDER].\n\n";
            }
        }
        
        if(line.find("SAFETY")!=std::string::npos) 
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            if (checkStringBool(out)==1)
            {
                hku = true;
            }
            else if (checkStringBool(out)==0)
            {
                std::cout << "[SAFETY] is present, and turned off. I hope to god you know what you're doing!\n\n";
            }
            else
            {
                std::cout << "[SAFETY]: [Non Boolean value]. Use something like: 1, 0, True, False, TRUE, FALSE, true, false. \n";
                std::cout << "Using panic behavior : True! Check settings.ini!\n\n";
                hku = true;
            }
        }

        if(line.find("RUN")!=std::string::npos) 
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            std::cout << "[RUN] found! :";
            if (checkStringBool(out)==1)
            {
                std::cout << " True! We'll run.\n\n";
            }
            else if (checkStringBool(out)==0)
            {
                run = false;
                std::cout << " False! \n\nFiles at [DESTINATION FOLDER] will !!NOT!! be deleted when they don't match [FROM FOLDER].\n\n";
            }
            else
            {
                std::cout << " [Non Boolean value]. Use something like: 1, 0, True, False, TRUE, FALSE, true, false. \n";
                std::cout << "Using panic behavior : False! We won't run the service. Check settings.ini!\n\n";
                run = false;
            }
        }

    }
    file.close();

    if (!found_from) std::cout << "[ERROR]: No [FROM FOLDER] tag found, or path is invalid.\n\nSpecify one with: [FROM FOLDER] : C:/path/to/folder\nin \"settings.ini\" .\n\n";
    if (!found_to) std::cout << "[ERROR]: No [DESTINATION FOLDER] tag found, or path is invalid.\n Specify one with: [DESTINATION FOLDER] : C:/path/to/folder\nin \"settings.ini\" .\n\n";
    if (!found_timer) std::cout << "[ERROR]: No [DELAY] timer tag found.\n Specify one with: [DELAY] : 8 \n\n(this example sets the timer to 8 seconds)\nin \"settings.ini\" .\n";

    if(found_from && found_to && found_timer)
    {
        std::cout << "Read all nessecary attributes from \"settings.ini\".\n\n";
        if(hku)
        {
            if(folder_in.string().find("X:/")==std::string::npos)
            {
                std::cout << "[SAFETY]: " << folder_in.string() << "is not on X Drive. Not resuming operation.\n\n";
            }
        }
        else if (folder_in.string()==folder_to.string())
        {
            std::cout << "[ERROR]: FROM and DESTINATION folders are the same. This is invalid.\n\n";
        }
        else
        {
        setup_ok = 1;
        }
    }
}

//making INIstruct as part of a list.
INIstruct(std::fstream& file,std::string& _name)
{
    name = _name;
    std::string line;
    bool found_from = false;
    bool found_to = false;
    bool found_timer = false;
    //std::cout << "READING \"settings.ini\".\n\n";
    while(getline(file,line))
    {
        if(line.find("#")!=std::string::npos || line.length() == 0) continue; //spaces or empty lines won't be tolerated

        if(line.find("FROM")!=std::string::npos && !found_from)
        {
            size_t pos = line.find(":");
            std::string in = line.substr(pos+1);
            normalizePath(in);
            std::cout << in << " is found as [FROM FOLDER], "; 
            folder_in = std::filesystem::path(in);
            if(std::filesystem::exists(folder_in))
            {
                std::cout << "and it exists! \n\n";
                found_from = true;
            }
            else
            {
                std::cout << "and it does NOT exist! \n\n";
                found_from = false;
            }
        }

        if(line.find("DESTINATION")!=std::string::npos && !found_to)
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            normalizePath(out);
            std::cout << out << " is found as [DESTINATION FOLDER], "; 
            folder_to = std::filesystem::path(out);
            if(std::filesystem::exists(folder_to))
            {
                std::cout << "and it exists! \n\n";
                found_to = true;
            }
            else
            {
                std::cout << "and it does NOT exist! \n\n";
                found_to = false;
            }
        }

        if(line.find("DELAY")!=std::string::npos && !found_timer)
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            remove_leadingWhiteSpace(out);
            try
            {
                std::cout << "[DELAY] Timer found: " << stoi(out) << " seconds\n\n"; 
                wait_time = std::chrono::seconds(stoi(out));
                found_timer = true;
            }
            catch(std::invalid_argument const& except)
            {
                std::cout << "[DELAY] flag found, but value: \"" << out << "\" wasn't suitable for conversion, because is it not a number.\nSetting [DELAY] to: 30 seconds.\n\n";
                wait_time = std::chrono::seconds(30);
                found_timer = true;
            }
            catch(std::out_of_range const& except)
            {
                std::cout << "[DELAY] flag found, but value: \"" << out << "\" is out of range.\nSetting [DELAY] to: 30 seconds.\n\n";
                wait_time = std::chrono::seconds(30);
                found_timer = true;
            }
            
        }

        //Delete non matching 
        if(line.find("DELETE NON-MATCHING")!=std::string::npos) 
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            std::cout << "[DELETE NON MATCHING] found! :";
            if (checkStringBool(out)==1)
            {
                del_to = true;
                std::cout << " True! \n\nFiles at [DESTINATION FOLDER] will be deleted when they don't match [FROM FOLDER].\n\n";
            }
            else if (checkStringBool(out)==0)
            {
                del_to = false;
                std::cout << " False! \n\nFiles at [DESTINATION FOLDER] will !!NOT!! be deleted when they don't match [FROM FOLDER].\n\n";
            }
            else
            {
                std::cout << " [Non Boolean value]. Use something like: 1, 0, True, False, TRUE, FALSE, true, false. \n";
                std::cout << "Using Default behavior : True.\nFiles at [DESTINATION FOLDER] will be deleted when they don't match [FROM FOLDER].\n\n";
            }
        }
        
        if(line.find("SAFETY")!=std::string::npos) 
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            if (checkStringBool(out)==1)
            {
                hku = true;
            }
            else if (checkStringBool(out)==0)
            {
                std::cout << "[SAFETY] is present, and turned off. I hope to god you know what you're doing!\n\n";
            }
            else
            {
                std::cout << "[SAFETY]: [Non Boolean value]. Use something like: 1, 0, True, False, TRUE, FALSE, true, false. \n";
                std::cout << "Using panic behavior : True! Check settings.ini!\n\n";
                hku = true;
            }
        }

        if(line.find("RUN")!=std::string::npos) 
        {
            size_t pos = line.find(":");
            std::string out = line.substr(pos+1);
            std::cout << "[RUN] found! :";
            if (checkStringBool(out)==1)
            {
                std::cout << " True! We'll run.\n\n";
            }
            else if (checkStringBool(out)==0)
            {
                run = false;
                std::cout << " False! \nService will not be ran.\n\n";
            }
            else
            {
                std::cout << " [Non Boolean value]. Use something like: 1, 0, True, False, TRUE, FALSE, true, false. \n";
                std::cout << "Using panic behavior : False! We won't run the service. Check settings.ini!\n\n";
                run = false;
            }
        }

        if(line.find("[SERVICE END]")!=std::string::npos) 
        { 
            //end of block.
            break;
        }

    }

    if (!found_from) std::cout << "[ERROR]: No [FROM FOLDER] tag found, or path is invalid.\n\nSpecify one with: [FROM FOLDER] : C:/path/to/folder\nin \"settings.ini\" .\n\n";
    if (!found_to) std::cout << "[ERROR]: No [DESTINATION FOLDER] tag found, or path is invalid.\n Specify one with: [DESTINATION FOLDER] : C:/path/to/folder\nin \"settings.ini\" .\n\n";
    if (!found_timer) std::cout << "[ERROR]: No [DELAY] timer tag found.\n Specify one with: [DELAY] : 8 \n\n(this example sets the timer to 8 seconds)\nin \"settings.ini\" .\n";

    if(found_from && found_to && found_timer)
    {
        std::cout << "Read all nessecary attributes from \"settings.ini\".\n\n";
        if(hku)
        {
            if(folder_in.string().find("X:/")==std::string::npos)
            {
                std::cout << "[SAFETY]: " << folder_in.string() << "is not on X Drive. Not resuming operation.\n\n";
            }
        }
        else if (folder_in.string()==folder_to.string())
        {
            std::cout << "[ERROR]: FROM and DESTINATION folders are the same. This is invalid.\n\n";
        }
        else
        {
        setup_ok = 1;
        }
    }
    }
};

struct SettingsList
{
    std::vector<INIstruct> services = {};
    bool run = 1;

    SettingsList()
    {
        std::fstream file("settings.ini");
        std::cout << "READING \"settings.ini\".\n\n";
        
        std::string line;
        while(getline(file,line))
        {
            if(line.find("#")!=std::string::npos || line.length() == 0) continue; //spaces or empty lines won't be tolerated

            //add service if begin service block is found
            if(line.find("[SERVICE]")!=std::string::npos) 
            {
                //fetching name
                size_t pos = line.find(":");
                std::string out = line.substr(pos+1);
                remove_leadingWhiteSpace(out);
                remove_trailingWhiteSpace(out);
                std::cout << "\nService: "<< out << "\n" << "-----------------------------------------------------\n\n";

                auto entry = INIstruct(file,out);

                services.push_back(entry);
            }

            if(line.find("[GLOBAL RUN]")!=std::string::npos) 
            {
                size_t pos = line.find(":");
                std::string out = line.substr(pos+1);
                std::cout << "[GLOBAL RUN] found! :";
                if (checkStringBool(out)==1)
                {
                    std::cout << " True! We'll run services unless they have their [RUN] flag set to 0.\n\n";
                }
                else if (checkStringBool(out)==0)
                {
                    run = false;
                    std::cout << " False! \nServices will not be ran.\n\n";
                }
                else
                {
                    std::cout << " [Non Boolean value]. Use something like: 1, 0, True, False, TRUE, FALSE, true, false. \n";
                    std::cout << "Using panic behavior : False! We won't run the services. Check settings.ini!\n\n";
                    run = false;
                }
            }
        }
        file.close();
    }

};

void waiting_animation_dots(std::atomic_bool& cancel_token)
    {
        auto time = std::chrono::milliseconds(500);
        int count = 0;
        std::cout << "Waiting";
        while(cancel_token == false)
        {
            if(count < 5)
            {
                std::cout << '.';
            }
            else
            {
                std::cout << '\r';
                std::cout << "             ";
                std::cout << '\r';
                count = 0;
                std::cout << "Waiting";
            }
            std::this_thread::sleep_for(time);
            count++;
        }
    }

void job_sync(INIstruct& settings)
{
    //setting some options for updating files when nessecary
    std::filesystem::copy_options cpy_opions = std::filesystem::copy_options::update_existing;

    for(std::filesystem::directory_entry dir : std::filesystem::recursive_directory_iterator(settings.folder_in))
    {
        std::string cur_path = dir.path().string();
        normalizePath(cur_path);

        //figure out equivaluent path on the other side.
        std::string rel_to_path = settings.folder_to.string()+cur_path.substr(settings.folder_in.string().length());

        //if it doesn't exist, let's make it.
        if(!std::filesystem::exists(rel_to_path))
        {
            std::cout << rel_to_path << " does not exist.\n";
            if(std::filesystem::is_directory(cur_path))                                 //if current item is a directory, make a new directory. 
            {                                                                           //We don't have to call create_directories due to the recursive nature of this iterator.
                std::filesystem::create_directory(rel_to_path);
            }
            else if (std::filesystem::is_regular_file(cur_path))                        //if current item is a file, copy it.
            {
                std::filesystem::copy_file(cur_path,rel_to_path);
            }
        }
        else                                                                        //if the currnt item DOES exist, we need to see if we have to update it.
        {
            if (std::filesystem::is_regular_file(cur_path))
            {
                auto time_in_file = std::filesystem::last_write_time(cur_path);
                auto time_to_file = std::filesystem::last_write_time(rel_to_path);
                
                if(time_in_file>time_to_file)                                           //The file in the IN folder is older, so the file in the TO folder must be replaced.
                {
                    std::filesystem::copy(cur_path,rel_to_path,cpy_opions);             //use copy options we made earlier to replace te file.
                }
            }
        }
    }

    //now that everything is copied, we must make sure to delete whatever is extra in the TO folder, if the settings allow for it. (by default, yes.)
    //in INI file, define this with [DELETE NON-MATCHING] : 1, or True
    //or [DELETE NON-MATCHING] : 0, or False
    if(settings.del_to)
    {
    for(std::filesystem::directory_entry dir : std::filesystem::recursive_directory_iterator(settings.folder_to))
    {
        std::string to_path = dir.path().string();
        normalizePath(to_path);
        //figure out equivaluent path on the IN side.
        std::string rel_in_path = settings.folder_in.string()+to_path.substr(settings.folder_to.string().length());
    
        if(!std::filesystem::exists(rel_in_path))                                       //if IN side equivalent of something here doesn't exist, delete it. 
        {
            std::filesystem::remove_all(to_path);
        }
    }
    }
}

void service_sync(INIstruct& settings,std::mutex& mut)
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
    auto time = std::chrono::high_resolution_clock::now();
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
    std::cout << "[SERVICE JOB END] - Sync service " << settings.name << " ended at: " << std::ctime(&nowt2) ;
    std::cout << "---------------------------------------------------------------------------------\n";
    std::cout << "- Service " << settings.name <<" took: " << seconds.count() <<" seconds\n";
    std::cout << "- Waiting " << settings.wait_time.count() << " seconds for next run of " << settings.name<<".\n\n";
    }
}

void service_routine(INIstruct& settings,std::mutex& mut)
{
    while(true)
    {
        auto fut = std::async(std::launch::async,service_sync,std::ref(settings),std::ref(mut));
        std::this_thread::sleep_for(settings.wait_time);
    }
}

//notes
//std::filesystem::last_write_time() << check if file needs to be copied and overwritten, if last write time in the "IN" folder is larger than in the "TO" folder

int main()
{
    //make console window invisible with windows.h
    HWND hWnd = GetConsoleWindow();
    ShowWindow( hWnd, SW_HIDE );

    SettingsList service_list;

    if (service_list.run==0)
    {
        std::cout << "[GLOBAL RUN] flag set to False, not running services.\n";
        std::cin.get();
        return 0;
    }

    std::mutex service_mutex;

    std::vector<std::future<void>> futurelist;

    for(auto& settings : service_list.services)
    {
        if (settings.setup_ok==0)
        {
            std::cout << "Service: " << settings.name << "encountered errors, quitting...\n";
            continue;
        }

        if (settings.run==0)
        {
            std::cout << "[RUN] flag set to False, not running service.\n";
            continue;
        }

        std::cout << "[SERVICE START] - Starting service: "<< settings.name <<"...\n";

        futurelist.push_back(std::async(std::launch::async,service_routine,std::ref(settings),std::ref(service_mutex)));
    }

return 0;
}