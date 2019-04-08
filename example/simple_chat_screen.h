/** @file
 *  
 *  Screen layout constants
 * 
 *  @author Thurman Gillespy
 */

#include <iostream>
#include <string>
#include <vector>

class simple_chat_screen
{
private:
    const std::string m_ip_addr;
    const std::string m_port;
    const std::string m_connect_type;
    std::string m_upper_screen;
    std::string m_scroll_text;

public:
    simple_chat_screen(const std::string ip, const std::string port, const std::string type) :
        m_ip_addr(ip), m_port(port), m_connect_type(type) {
        create_upper_screen();
        create_scroll_text();
    };

    void draw_screen() {
        // std::string screen_out = m_upper_screen + m_scroll_text + BOTTOM;
        std::system("clear");
        std::cout << (m_upper_screen + m_scroll_text + BOTTOM);
    }

    void insert_scroll_line(const std::string text, const std::string prefix = "") {
        // create the new scroll line, remove '\n' in text
        std::string new_scroll_line = "| " + prefix + text.substr(0, text.size() - 1);
        // add the scroll line ending
        new_scroll_line += 
            BLANK_LINE.substr(new_scroll_line.size(), std::string::npos);
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

    void create_upper_screen() {
        m_upper_screen =
            TOP + BLANK_LINE + HDR_1 + DIVIDOR + BLANK_LINE +
            HDR_IP + (m_ip_addr == "" ? "\"\"" : m_ip_addr) + HDR_PORT + 
            m_port + "\n" + HDR_TYPE + 
            (m_connect_type == PARAM_ACCEPT ? ACCEPT_T : CONNECT_T) +
            DIVIDOR + BLANK_LINE + HDR_INSTR + DIVIDOR;
    }

    void create_scroll_text() {
        m_scroll_text = BLANK_LINE + BLANK_LINE + BLANK_LINE + BLANK_LINE + BLANK_LINE +
            BLANK_LINE + BLANK_LINE + BLANK_LINE + BLANK_LINE + BLANK_LINE;
    }
};
