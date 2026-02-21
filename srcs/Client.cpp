/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: chanypar <chanypar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 18:43:06 by chanypar          #+#    #+#             */
/*   Updated: 2026/02/21 18:43:07 by chanypar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client() : fd(-1), authed(false), passOk(false), isTerminating(false){}

Client::Client(int fd) : fd(fd), authed(false), passOk(false), isTerminating(false) {}

