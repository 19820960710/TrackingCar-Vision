#include "../vision_packet.h"

#include <assert.h>
#include <stddef.h>

static void test_tv_target_at_origin(void)
{
    vision_packet_t packet;
    vision_observation_t observation;

    assert(vision_packet_parse_line("TV,1,-256,-160,0,0,perspective", &packet));
    assert(packet.protocol == VISION_OBSERVATION_PROTOCOL_TV);
    assert(packet.target_valid);
    assert(vision_packet_to_observation(&packet, 512U, 320U, 1U, 10U,
                                        &observation));
    assert(observation.target_valid);
    assert(observation.target_x == 0U);
    assert(observation.target_y == 0U);
}

static void test_aim_target_without_laser(void)
{
    vision_packet_t packet;

    assert(vision_packet_parse_line(
        "AIM,0,12,-8,244,168,0,0,perspective,NO_LASER", &packet));
    assert(packet.protocol == VISION_OBSERVATION_PROTOCOL_AIM);
    assert(packet.target_valid);
    assert(!packet.laser_valid);
}

static void test_protocol_validity_consistency(void)
{
    vision_packet_t packet;

    assert(!vision_packet_parse_line(
        "AIM,1,0,0,1,2,3,4,LOST,NO_LASER", &packet));
    assert(vision_packet_parse_line(
        "AIM,0,0,0,0,0,0,0,NO_TARGET,NO_LASER", &packet));
    assert(!packet.target_valid);
    assert(vision_packet_parse_line("TV,1,0,0,1,2,LOST", &packet));
    assert(!packet.target_valid);
}

static void test_observation_freshness(void)
{
    vision_observation_t observation = {
        .received_at_ms = UINT32_MAX - 4U,
        .protocol = VISION_OBSERVATION_PROTOCOL_TV,
    };

    assert(vision_observation_is_fresh(&observation, 3U, 8U));
    assert(!vision_observation_is_fresh(&observation, 4U, 8U));
    assert(!vision_observation_is_fresh(NULL, 4U, 8U));
}

static void test_invalid_lines(void)
{
    vision_packet_t packet;
    vision_observation_t observation;

    assert(!vision_packet_parse_line("AIM,1,0,0,1,2,3,4", &packet));
    assert(vision_packet_parse_line("TV,1,0,0,512,1,perspective", &packet));
    assert(!vision_packet_to_observation(&packet, 512U, 320U, 1U, 10U,
                                         &observation));
    assert(!vision_packet_parse_line("TV,1,0,0,1,2,mode,extra", &packet));
    assert(!vision_packet_parse_line("", &packet));
    assert(!vision_packet_parse_line("\n", &packet));
    assert(!vision_packet_parse_line(
        "TV,999999999999999999999,0,0,1,2,perspective", &packet));
}

int main(void)
{
    test_tv_target_at_origin();
    test_aim_target_without_laser();
    test_protocol_validity_consistency();
    test_observation_freshness();
    test_invalid_lines();
    return 0;
}
