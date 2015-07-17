/**
  * @brief contains main for the backplane application
  * @file backplane.cpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

#include "acs/CBackplane.hpp"

int main(int argc, char **argv) {
    airware::acs::CBackplane bp;
    return bp.main(argc, argv);
}

