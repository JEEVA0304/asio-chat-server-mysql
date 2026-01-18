#include <asio.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sstream>
#include <mysql.h>
#include <vector>
#include <algorithm>

using asio::ip::tcp;

class Session; // Forward declaration

// Global variables
asio::io_context io_context;
std::map<std::string, std::shared_ptr<Session>> users;

class ChatDatabase {
private:
    MYSQL* conn;

public:
    ChatDatabase() {
        conn = mysql_init(NULL);
        if (!mysql_real_connect(conn, "localhost", "USERNAME", "PASSWORD", "chatdb", 3306, NULL, 0)) {
            //chatdb is database 
            std::cout << "Database connection failed: " << mysql_error(conn) << std::endl;
            exit(1);
        }
        std::cout << "Connected to MySQL database!" << std::endl;
        create_tables();
    }
    
    ~ChatDatabase() {
        mysql_close(conn);
    }
    
    void create_tables() {
        const char* create_users = "CREATE TABLE IF NOT EXISTS users ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "username VARCHAR(50) UNIQUE NOT NULL, "
            "password VARCHAR(100) NOT NULL, "
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")";
        
        const char* create_messages = "CREATE TABLE IF NOT EXISTS messages ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "from_user VARCHAR(50) NOT NULL, "
            "to_user VARCHAR(50) DEFAULT NULL, "
            "message TEXT NOT NULL, "
            "is_private BOOLEAN DEFAULT FALSE, "
            "sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")";
            
        mysql_query(conn, create_users);
        mysql_query(conn, create_messages);
        std::cout << "Database tables ready!" << std::endl;
    }
    
    bool register_user(const std::string& username, const std::string& password) {
        std::string query = "INSERT INTO users (username, password) VALUES ('" 
            + username + "', '" + password + "')";
        
        return mysql_query(conn, query.c_str()) == 0;
    }
    
    bool authenticate_user(const std::string& username, const std::string& password) {
        std::string query = "SELECT password FROM users WHERE username = '" + username + "'";
        
        if (mysql_query(conn, query.c_str())) {
            return false;
        }
        
        MYSQL_RES* res = mysql_store_result(conn);
        if (res == NULL) return false;
        
        MYSQL_ROW row = mysql_fetch_row(res);
        bool valid = (row && std::string(row[0]) == password);
        
        mysql_free_result(res);
        return valid;
    }
    
    void save_message(const std::string& from_user, const std::string& message, 
                     const std::string& to_user = "", bool is_private = false) {
        std::string query = "INSERT INTO messages (from_user, to_user, message, is_private) VALUES ('" 
            + from_user + "', '" + (is_private ? to_user : "") + "', '" + message + "', " 
            + (is_private ? "TRUE" : "FALSE") + ")";
            
        mysql_query(conn, query.c_str());
    }
    
    std::vector<std::string> get_recent_messages(int limit = 10) {
        std::string query = "SELECT from_user, message, sent_at FROM messages "
            "WHERE is_private = FALSE ORDER BY sent_at DESC LIMIT " + std::to_string(limit);
            
        std::vector<std::string> messages;
        
        if (mysql_query(conn, query.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(conn);
            MYSQL_ROW row;
            
            while ((row = mysql_fetch_row(res))) {
                messages.push_back("[" + std::string(row[0]) + "]: " + std::string(row[1]));
            }
            
            mysql_free_result(res);
        }
        
        std::reverse(messages.begin(), messages.end());
        return messages;
    }
};

// Global database instance
ChatDatabase db;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        auto recent_messages = db.get_recent_messages(5);
        deliver("=== Recent Chat History ===");
        for (const auto& msg : recent_messages) {
            deliver(msg);
        }
        deliver("=== End History ===");
        do_read_line();
    }

    void deliver(const std::string& msg) {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(msg + "\n"),
            [this, self](std::error_code /*ec*/, std::size_t) {});
    }

private:
    void do_read_line() {
        auto self(shared_from_this());
        asio::async_read_until(socket_, buffer_, '\n',
            [this, self](std::error_code ec, std::size_t) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string line;
                    std::getline(is, line);
                    
                    if (!logged_in_) {
                        handle_login(line);
                    } else {
                        handle_message(line);
                    }
                    do_read_line();
                } else {
                    if (logged_in_) {
                        users.erase(username_);
                        broadcast(username_ + " left the chat.");
                    }
                }
            });
    }

    void handle_login(const std::string& line) {
        if (line.rfind("LOGIN ", 0) == 0) {
            std::istringstream iss(line.substr(6));
            std::string uname, pwd;
            iss >> uname >> pwd;

            if (db.authenticate_user(uname, pwd)) {
                deliver("Logged in as " + uname);
            } else {
                if (db.register_user(uname, pwd)) {
                    deliver("Registered and logged in as " + uname);
                } else {
                    deliver("Invalid credentials or username taken.");
                    return;
                }
            }

            username_ = uname;
            logged_in_ = true;
            users[username_] = shared_from_this();
            
            std::string join_msg = username_ + " joined the chat.";
            broadcast(join_msg);
            db.save_message("SYSTEM", username_ + " joined the chat.", "", false);
        } else {
            deliver("Please login first using: LOGIN <username> <password>");
        }
    }

    void handle_message(const std::string& msg) {
        if (msg.rfind("/msg ", 0) == 0) {
            std::istringstream iss(msg.substr(5));
            std::string target_user;
            iss >> target_user;
            std::string private_msg;
            std::getline(iss, private_msg);

            if (users.count(target_user)) {
                users[target_user]->deliver("[PM from " + username_ + "]:" + private_msg);
                deliver("[PM to " + target_user + "]:" + private_msg);
                db.save_message(username_, private_msg, target_user, true);
            } else {
                deliver("User " + target_user + " not found.");
            }
        } else if (msg == "/list") {
            std::string list = "Online users: ";
            for (auto &p : users) {
                list += p.first + " ";
            }
            deliver(list);
        } else if (msg == "/history") {
            deliver("=== Recent Messages ===");
            auto recent = db.get_recent_messages(20);
            for (const auto& m : recent) {
                deliver(m);
            }
        } else {
            std::string full_msg = "[" + username_ + "]: " + msg;
            broadcast(full_msg);
            db.save_message(username_, msg, "", false);
        }
    }

    void broadcast(const std::string& msg) {
        for (auto& p : users) {
            if (p.second.get() != this) {
                p.second->deliver(msg);
            }
        }
    }

    tcp::socket socket_;
    asio::streambuf buffer_;
    std::string username_;
    bool logged_in_ = false;
};

int main() {
    try {
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 12345));
        std::cout << "Chat server with MySQL started on port 12345\n";

        std::function<void()> do_accept;
        do_accept = [&]() {
            acceptor.async_accept([&](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                do_accept();
            });
        };

        do_accept();
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }
    return 0;
}
