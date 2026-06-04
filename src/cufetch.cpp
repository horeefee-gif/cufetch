#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <memory>
#include <array>
#include <unistd.h> 
#include <climits> 

struct SysInfo {
    std::string user;
    std::string title;
    std::string os;
    std::string kernel;
    std::string host;
    unsigned hours = 0;
    unsigned minutes = 0;
    std::string pkgs;
    std::string memory;
    std::string cpu;
    std::string gpu;
    std::string distro;
};

std::string exec_command(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "Error";
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

std::string get_distro() {
    std::ifstream in("/etc/os-release");
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("ID_LIKE=", 0) == 0) { 
            std::string value = line.substr(8);
            return value;
        }
    }
    return "Linux";
}

std::string get_title() {
    std::ifstream in("/etc/hostname");
    std::string title;
    if (in && std::getline(in, title)) return title;
    return "Unknown";
}

std::string get_os() {
    std::ifstream in("/etc/os-release");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("NAME=", 0) == 0) { 
            std::string value = line.substr(5);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            return value;
        }
    }
    return "Linux";
}

std::string get_host() {
    std::ifstream in("/sys/devices/virtual/dmi/id/product_name");
    std::string host;
    if (in && std::getline(in, host)) return host;
    return "Unknown";
}

void get_uptime(unsigned& hours, unsigned& minutes) {
    std::ifstream in("/proc/uptime");
    double uptime_seconds;
    if (in >> uptime_seconds) {
        unsigned total_seconds = static_cast<unsigned>(uptime_seconds);
        hours = total_seconds / 3600;
        minutes = (total_seconds % 3600) / 60;
    }
}

std::string get_memory() {
    std::ifstream in("/proc/meminfo");
    std::string token;
    unsigned long mem_total = 0, mem_available = 0;

    while (in >> token) {
        if (token == "MemTotal:") {
            in >> mem_total;
        } else if (token == "MemAvailable:") {
            in >> mem_available;
        }
        if (mem_total && mem_available) break;
    }

    if (mem_total && mem_available) {
        double used_gb = (mem_total - mem_available) / 1024.0 / 1024.0;
        double total_gb = mem_total / 1024.0 / 1024.0;
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2fG / %.2fG", used_gb, total_gb);
        return std::string(buf);
    }
    return "Error";
}

std::string get_cpu() {
    std::ifstream in("/proc/cpuinfo");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("model name", 0) == 0) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string cpu = line.substr(colon_pos + 1);
                // Удаляем лишние начальные пробелы
                cpu.erase(0, cpu.find_first_not_of(" \t"));
                return cpu;
            }
        }
    }
    return "Unknown";
}

std::string get_gpu() {
    std::string res = exec_command("lspci | grep -E \"VGA|3D\"");
    size_t first = res.find('[');
    size_t last = res.find(']');
    if (first != std::string::npos && last != std::string::npos && first < last) {
        return res.substr(first + 1, last - first - 1);
    }
    return "Unknown";
}

std::string get_user() {
    char* user = std::getenv("USER");
    if (user) return std::string(user);
    
    char login_buf[LOGIN_NAME_MAX];
    if (getlogin_r(login_buf, sizeof(login_buf)) == 0) {
        return std::string(login_buf);
    }
    return "user";
}

int main() {
    SysInfo info;
    info.user = get_user();
    info.title = get_title();
    info.os = get_os();
    info.kernel = exec_command("uname -r");
    info.host = get_host();
    get_uptime(info.hours, info.minutes);
    info.distro = get_distro();

    if (info.distro == "arch") {
        info.pkgs = exec_command("pacman -Q | wc -l");
    } else if (info.distro == "fedora") {
        info.pkgs = exec_command("rpm -qa | wc -l"); 
    } else if (info.distro == "debian") {
        info.pkgs = exec_command("dpkg -l | grep ^ii | wc -l");
    }

    info.memory = get_memory();
    info.cpu = get_cpu();
    info.gpu = get_gpu();

    std::cout << "\n";
    std::cout << "                " << info.user << "@" << info.title << "\n";
    std::cout << "\n";
    std::cout << "     .--."     << "\tOS: " << info.os << "\n";
    std::cout << "    |o_o |:"    << "\tHost: " << info.host << "\n";
    std::cout << "    |:_/ |"     << "\tKernel: " << info.kernel << "\n";
    std::cout << "   //   \\ \\"   << "\tUptime: " << info.hours << "h " << info.minutes << "m\n";
    std::cout << "  (|     | )"   << "\tPackages: " << info.pkgs << "\n";
    std::cout << "  /'\\_   _/`\\"  << "\tMemory: " << info.memory << "\n";
    std::cout << "  \\___)=(___/"  << "\tCPU: " << info.cpu << "\n";
    std::cout << "             " << "\tGPU: " << info.gpu << "\n\n";

    return 0;
}
