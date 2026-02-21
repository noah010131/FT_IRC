/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: chanypar <chanypar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 18:43:03 by chanypar          #+#    #+#             */
/*   Updated: 2026/02/21 18:43:04 by chanypar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
#include <cstdlib>

int main(int ac, char **av) {
    if (ac != 3) {
        std::cerr << "Usage: ./ircserv <port> <password>\n";
        return 1;
    }
    int port = std::atoi(av[1]);
    std::string password = av[2];

    try {
        Server server(port, password);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
