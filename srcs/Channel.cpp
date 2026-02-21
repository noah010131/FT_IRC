/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: chanypar <chanypar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/21 18:43:09 by chanypar          #+#    #+#             */
/*   Updated: 2026/02/21 18:43:10 by chanypar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

std::string Channel::getModes() {
    std::string modes = "+";
    std::string params = "";

    if (inviteOnly) modes += "i";
    if (topicRestricted) modes += "t";
    if (!key.empty()) {
        modes += "k";
        params += " " + key;
    }
    if (userLimit > 0) {
        modes += "l";
        std::stringstream ss;
		ss << userLimit;
		params += " " + ss.str();
    }
    return modes + params;
}

std::string Channel::getCreationTime()
{
    std::stringstream ss;
    ss << _createdAt;
    return ss.str();
}