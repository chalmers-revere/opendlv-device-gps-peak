/*
 * Copyright (C) 2019  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"
#include "peak_can.hpp"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>

#ifdef __linux__
    #include <linux/if.h>
    #include <linux/can.h>
#endif

#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{1};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("cid")) ) {
        std::cerr << argv[0] << " translates messages from CAN to ODVD messages for PEAK CAN GPS." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid=<OD4 session> [--id=ID] --can=<name of the CAN device> [--verbose]" << std::endl;
        std::cerr << "         --cid:    CID of the OD4Session to send and receive messages" << std::endl;
        std::cerr << "         --id:     ID to use as senderStamp for sending" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid=111 --can=can0 --verbose" << std::endl;
    }
    else {
        const std::string CANDEVICE{commandlineArguments["can"]};
        const uint32_t ID{(commandlineArguments["id"].size() != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};

        cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

        // Delegate to convert incoming CAN frames into ODVD messages that are broadcast into the OD4Session.
        opendlv::proxy::AngularVelocityReading msgAngularVelocityReading;
        auto decode = [&od4, VERBOSE, ID, &msgAngularVelocityReading](cluon::data::TimeStamp ts, uint16_t canFrameID, uint8_t *src, uint8_t len) {
            if ( (nullptr == src) || (0 == len) ) return;
            if (PEAK_CAN_GPS_COURSE_SPEED_FRAME_ID == canFrameID) {
                peak_can_gps_course_speed_t tmp;
                if (0 == peak_can_gps_course_speed_unpack(&tmp, src, len)) {
                    {
                        opendlv::proxy::GroundSpeedReading msg;
                        msg.groundSpeed(static_cast<float>(peak_can_gps_course_speed_gps_speed_decode(tmp.gps_speed)/3.6f));
                        if (VERBOSE) {
                            std::stringstream sstr;
                            msg.accept([](uint32_t, const std::string &, const std::string &) {},
                                       [&sstr](uint32_t, std::string &&, std::string &&n, auto v) { sstr << n << " = " << v << '\n'; },
                                       []() {});
                            std::cout << sstr.str() << std::endl;
                        }
                        od4.send(msg, ts, ID);
                    }

                    {
                        opendlv::proxy::GeodeticHeadingReading msg;
                        msg.northHeading(static_cast<float>(peak_can_gps_course_speed_gps_course_decode(tmp.gps_course)/180.0f * M_PI));
                        if (VERBOSE) {
                            std::stringstream sstr;
                            msg.accept([](uint32_t, const std::string &, const std::string &) {},
                                       [&sstr](uint32_t, std::string &&, std::string &&n, auto v) { sstr << n << " = " << v << '\n'; },
                                       []() {});
                            std::cout << sstr.str() << std::endl;
                        }
                        od4.send(msg, ts, ID);
                    }
                }
            }
            else if (PEAK_CAN_BMC_ACCELERATION_FRAME_ID == canFrameID) {
                peak_can_bmc_acceleration_t tmp;
                if (0 == peak_can_bmc_acceleration_unpack(&tmp, src, len)) {
                    {
                        opendlv::proxy::TemperatureReading msg;
                        msg.temperature(static_cast<float>(peak_can_bmc_acceleration_temperature_decode(tmp.temperature)));
                        if (VERBOSE) {
                            std::stringstream sstr;
                            msg.accept([](uint32_t, const std::string &, const std::string &) {},
                                       [&sstr](uint32_t, std::string &&, std::string &&n, auto v) { sstr << n << " = " << v << '\n'; },
                                       []() {});
                            std::cout << sstr.str() << std::endl;
                        }
                        od4.send(msg, ts, ID);
                    }

                    {
                        opendlv::proxy::AccelerationReading msg;
                        msg.accelerationX(static_cast<float>(peak_can_bmc_acceleration_acceleration_x_decode(tmp.acceleration_x)));
                        msg.accelerationY(static_cast<float>(peak_can_bmc_acceleration_acceleration_y_decode(tmp.acceleration_y)));
                        msg.accelerationZ(static_cast<float>(peak_can_bmc_acceleration_acceleration_z_decode(tmp.acceleration_z)));
                        if (VERBOSE) {
                            std::stringstream sstr;
                            msg.accept([](uint32_t, const std::string &, const std::string &) {},
                                       [&sstr](uint32_t, std::string &&, std::string &&n, auto v) { sstr << n << " = " << v << '\n'; },
                                       []() {});
                            std::cout << sstr.str() << std::endl;
                        }
                        od4.send(msg, ts, ID);
                    }
                }
            }
            else if (PEAK_CAN_BMC_MAGNETIC_FIELD_FRAME_ID == canFrameID) {
                peak_can_bmc_magnetic_field_t tmp;
                if (0 == peak_can_bmc_magnetic_field_unpack(&tmp, src, len)) {
                    opendlv::proxy::MagneticFieldReading msg;
                    msg.magneticFieldX(static_cast<float>(peak_can_bmc_magnetic_field_magnetic_field_x_decode(tmp.magnetic_field_x)));
                    msg.magneticFieldY(static_cast<float>(peak_can_bmc_magnetic_field_magnetic_field_y_decode(tmp.magnetic_field_y)));
                    msg.magneticFieldZ(static_cast<float>(peak_can_bmc_magnetic_field_magnetic_field_z_decode(tmp.magnetic_field_z)));
                    if (VERBOSE) {
                        std::stringstream sstr;
                        msg.accept([](uint32_t, const std::string &, const std::string &) {},
                                   [&sstr](uint32_t, std::string &&, std::string &&n, auto v) { sstr << n << " = " << v << '\n'; },
                                   []() {});
                        std::cout << sstr.str() << std::endl;
                    }
                    od4.send(msg, ts, ID);
                }
            }
            else if (PEAK_CAN_L3_GD20_ROTATION_A_FRAME_ID == canFrameID) {
                peak_can_l3_gd20_rotation_a_t tmp;
                if (0 == peak_can_l3_gd20_rotation_a_unpack(&tmp, src, len)) {
                    msgAngularVelocityReading.angularVelocityX(static_cast<float>(peak_can_l3_gd20_rotation_a_rotation_x_decode(tmp.rotation_x)/180.0f * M_PI));
                    msgAngularVelocityReading.angularVelocityY(static_cast<float>(peak_can_l3_gd20_rotation_a_rotation_y_decode(tmp.rotation_y)/180.0f * M_PI));
                    if (VERBOSE) {
                        std::stringstream sstr;
                        msgAngularVelocityReading.accept([](uint32_t, const std::string &, const std::string &) {},
                                   [&sstr](uint32_t, std::string &&, std::string &&n, auto v) { sstr << n << " = " << v << '\n'; },
                                   []() {});
                        std::cout << sstr.str() << std::endl;
                    }
                    od4.send(msgAngularVelocityReading, ts, ID);
                }
            }
            else if (PEAK_CAN_L3_GD20_ROTATION_B_FRAME_ID == canFrameID) {
                peak_can_l3_gd20_rotation_b_t tmp;
                if (0 == peak_can_l3_gd20_rotation_b_unpack(&tmp, src, len)) {
                    msgAngularVelocityReading.angularVelocityZ(static_cast<float>(peak_can_l3_gd20_rotation_b_rotation_z_decode(tmp.rotation_z)/180.0f * M_PI));
                    if (VERBOSE) {
                        std::stringstream sstr;
                        msgAngularVelocityReading.accept([](uint32_t, const std::string &, const std::string &) {},
                                   [&sstr](uint32_t, std::string &&, std::string &&n, auto v) { sstr << n << " = " << v << '\n'; },
                                   []() {});
                        std::cout << sstr.str() << std::endl;
                    }
                    od4.send(msgAngularVelocityReading, ts, ID);
                }
            }
        };

#ifdef __linux__
        struct sockaddr_can address;
#endif
        int socketCAN;

        std::cerr << "[opendlv-device-gps-peak] Opening " << CANDEVICE << "... ";
#ifdef __linux__
        // Create socket for SocketCAN.
        socketCAN = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (socketCAN < 0) {
            std::cerr << "failed." << std::endl;

            std::cerr << "[opendlv-device-gps-peak] Error while creating socket: " << strerror(errno) << std::endl;
        }

        // Try opening the given CAN device node.
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strcpy(ifr.ifr_name, CANDEVICE.c_str());
        if (0 != ioctl(socketCAN, SIOCGIFINDEX, &ifr)) {
            std::cerr << "failed." << std::endl;

            std::cerr << "[opendlv-device-gps-peak] Error while getting index for " << CANDEVICE << ": " << strerror(errno) << std::endl;
            return retCode;
        }

        // Setup address and port.
        memset(&address, 0, sizeof(address));
        address.can_family = AF_CAN;
        address.can_ifindex = ifr.ifr_ifindex;

        if (bind(socketCAN, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {
            std::cerr << "failed." << std::endl;

            std::cerr << "[opendlv-device-gps-peak] Error while binding socket: " << strerror(errno) << std::endl;
            return retCode;
        }
        std::cerr << "done." << std::endl;
#else
        std::cerr << "failed (SocketCAN not available on this platform). " << std::endl;
        return retCode;
#endif

        struct can_frame frame;
        fd_set rfds;
        struct timeval timeout;
        struct timeval socketTimeStamp;
        int32_t nbytes = 0;

        while (od4.isRunning() && socketCAN > -1) {
#ifdef __linux__
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            FD_ZERO(&rfds);
            FD_SET(socketCAN, &rfds);

            select(socketCAN + 1, &rfds, NULL, NULL, &timeout);

            if (FD_ISSET(socketCAN, &rfds)) {
                nbytes = read(socketCAN, &frame, sizeof(struct can_frame));
                if ( (nbytes > 0) && (nbytes == sizeof(struct can_frame)) ) {
                    // Get receiving time stamp.
                    if (0 != ioctl(socketCAN, SIOCGSTAMP, &socketTimeStamp)) {
                        // In case the ioctl failed, use traditional vsariant.
                        cluon::data::TimeStamp now{cluon::time::now()};
                        socketTimeStamp.tv_sec = now.seconds();
                        socketTimeStamp.tv_usec = now.microseconds();
                    }
                    cluon::data::TimeStamp sampleTimeStamp;
                    sampleTimeStamp.seconds(socketTimeStamp.tv_sec)
                                   .microseconds(socketTimeStamp.tv_usec);
                    decode(sampleTimeStamp, frame.can_id, frame.data, frame.can_dlc);
                }
            }
#endif
        }

        std::clog << "[opendlv-device-gps-peak] Closing " << CANDEVICE << "... ";
        if (socketCAN > -1) {
            close(socketCAN);
        }
        std::clog << "done." << std::endl;

        retCode = 0;
    }
    return retCode;
}

