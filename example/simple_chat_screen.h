/** @file
 *  
 *  Screen layout constants
 * 
 *  @author Thurman Gillespy
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib> // std::system

static const int NUN_SCROLL_LINES = 10;

// https://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa
std::string string_to_hex(const std::string& input) {
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

class simple_chat_screen {
private:
    const std::string m_ip_addr;
    const std::string m_port;
    const std::string m_connect_type;
    std::string m_upper_screen;
    std::string m_scroll_text;
    const int m_num_scroll_lines;

public:
    simple_chat_screen(const std::string ip, const std::string port, const std::string type,
        int num_lines = NUN_SCROLL_LINES) :
        m_ip_addr(ip), m_port(port), m_connect_type(type), m_num_scroll_lines(num_lines) {
        create_upper_screen();
        create_scroll_text();
    };

    void draw_screen() {
        std::system("clear"); // BAD CODE
        std::cout << (m_upper_screen + m_scroll_text + BOTTOM);
    }

    void insert_scroll_line(const std::string& text, const std::string& prefix) {
        const std::string REMOTE = "[remote] "; // fix later
        
        // create the new scroll line
        // remove '\n' at end of text, and '\0' at beginning (if present)
        int first_pos = (text.find_first_of('\0') == 0 ? 1 : 0);
        std::string new_scroll_line = "| " + prefix + 
            text.substr(first_pos, text.size() - (1 + first_pos));
        // DEBUG: show string as ascii values
        // new_scroll_line += "<" + string_to_hex(text) + ">";
        new_scroll_line += 
            BLANK_LINE.substr(new_scroll_line.length(), std::string::npos);
        
        // remove old scroll line in scroll text (using substr), add new scroll line
        m_scroll_text =
            m_scroll_text.substr(m_scroll_text.find_first_of("\n") + 1, std::string::npos) +
            new_scroll_line;
    }

private:
    using S = const std::string;
    S PARAM_ACCEPT = "-accept"; // fix later
    S TOP =
        "\n_____________________________________________________________________________\n";
    S BLANK_LINE =
        "|                                                                           |\n";
    S DIVIDOR =
        "|___________________________________________________________________________|\n";
    S HDR_1 =
        "|                   chops-net-ip 2-way chat network demo                    |\n";
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
    S HDR_INSTR =
        "|   Enter text to send at prompt. Enter 'quit' to exit.                     |\n";
    S SCROLL_LINE =
        "|    ";
    S BOTTOM =
        "|---------------------------------------------------------------------------|\n";
    S HDR_START =
        "|  ";

    // create the string that represent the (unchanging) upper screen so only calculate once
    void create_upper_screen() {
        std::string hdr_info = HDR_IP + (m_ip_addr == "" ? "\"\"" : m_ip_addr) + HDR_PORT + 
            m_port ;
        hdr_info += BLANK_LINE.substr(hdr_info.size(), std::string::npos);
        m_upper_screen =
            TOP + BLANK_LINE + HDR_1 + DIVIDOR + BLANK_LINE + hdr_info + HDR_TYPE + 
            (m_connect_type == PARAM_ACCEPT ? ACCEPT_T : CONNECT_T) +
            DIVIDOR + BLANK_LINE + HDR_INSTR + DIVIDOR;
    }

    void create_scroll_text() {
        int count = m_num_scroll_lines;
        while (count-- > 0) {
            m_scroll_text += BLANK_LINE;
        }
    }
};
