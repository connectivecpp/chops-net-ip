/** @file
 *  
 *  @brief class to handle printing output to stdout for simple_chat_demo.cpp program.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/15/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef SIMPLE_CHAT_SCREEN_HPP_INCLUDED
#define SIMPLE_CHAT_SCREEN_HPP_INCLUDED

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib> // std::system

const int NUN_SCROLL_LINES = 10;

// shared globals
const std::string PARAM_CONNECT = "-connect";
const std::string PARAM_ACCEPT = "-accept";
const std::string REMOTE = "[remote] ";

// handle all methods to print output to stdout
class simple_chat_screen {
private:
    const std::string m_ip_addr; // IP address or host name
    const std::string m_port; // connection port
    const std::string m_connect_type; // @c '-connect' or @c '-accept'
    std::string m_upper_screen; // fixed upper region of screen output
    std::string m_scroll_text; // history scroll text region
    const int m_num_scroll_lines; // number of 'scroll lines'

public:
    simple_chat_screen(const std::string& ip, const std::string& port, const std::string& type,
        bool print_errors, int num_lines = NUN_SCROLL_LINES) :
        m_ip_addr(ip), m_port(port), m_connect_type(type), m_num_scroll_lines(num_lines) {
        create_upper_screen(print_errors);
        create_scroll_text();
    };

    // print the output to stdout
    // called after @c insert_scroll_line
    void draw_screen() {
        clear_screen();
        std::cout << (m_upper_screen + m_scroll_text + BOTTOM + PROMPT);
    }

    // the scroll region has a fixed numbmer of 'scroll lines'.
    // calculate complete new scroll line; delete old line at top
    // of text scroll region; add new scroll line (append)
    void insert_scroll_line(const std::string& text, const std::string& prefix) {
        
        // create the new scroll line
        // remove DELIM at end of text
        std::string new_scroll_line = "| " + prefix + 
            text.substr(0, text.size() - 1);
        new_scroll_line += 
            BLANK_LINE.substr(new_scroll_line.length(), std::string::npos);
        
        // remove old scroll line in scroll text (using substr), add new scroll line
        m_scroll_text =
            m_scroll_text.substr(m_scroll_text.find_first_of("\n") + 1, std::string::npos) +
            new_scroll_line;
    }

private:
    using S = const std::string;
    // string constants for output
    S TOP =
        "\n_____________________________________________________________________________\n";
    S BLANK_LINE =
        "|                                                                           |\n";
    S DIVIDOR =
        "|___________________________________________________________________________|\n";
    S HDR_1 =
        "|                      chops-net-ip chat network demo                       |\n";
    S HDR_IP =
        "|   IP address: ";
    S HDR_PORT =
        "    port: ";
    S HDR_TYPE =
        "|   connection type: ";
    S CONNECT_T =
        "connector                                              |\n";
    S ACCEPT_T =
        "acceptor                                               |\n";
    S ERR_LOG_ON = 
        "|     errors printed to console: ON                                         |\n";
    S ERR_LOG_OFF = 
        "|     errors printed to console: OFF                                        |\n";
    S HDR_INSTR =
        "|   Enter text to send at prompt. Enter 'quit' to exit.                     |\n";
    S SCROLL_LINE =
        "|    ";
    S BOTTOM =
        "|---------------------------------------------------------------------------|\n";
    S HDR_START =
        "|  ";
    S PROMPT = "| > ";

    // create the string that represent the (unchanging) upper screen so only calculate once
    void create_upper_screen(bool print_err) {
        std::string hdr_info = HDR_IP + (m_ip_addr == "" ? "\"\"" : m_ip_addr) + HDR_PORT + 
            m_port ;
        hdr_info += BLANK_LINE.substr(hdr_info.size(), std::string::npos);
        m_upper_screen =
            TOP + BLANK_LINE + HDR_1 + DIVIDOR + BLANK_LINE + hdr_info + HDR_TYPE + 
            (m_connect_type == PARAM_ACCEPT ? ACCEPT_T : CONNECT_T) + 
            (print_err ? ERR_LOG_ON : ERR_LOG_OFF) +
            DIVIDOR + BLANK_LINE + HDR_INSTR + DIVIDOR;
    }

    // seed the scroll text region with m_scoll_lines number of blank lines
    void create_scroll_text() {
        int count = m_num_scroll_lines;
        while (count-- > 0) {
            m_scroll_text += BLANK_LINE;
        }
    }

    // not recommended, but adequate for this demo
    // for problems with system("clear") see
    // http://www.cplusplus.com/articles/4z18T05o/
    void clear_screen() {
        #ifdef _WIN32
        system("cls");
        #else
        system("clear");
        #endif
    }
};

#endif