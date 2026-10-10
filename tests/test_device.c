/* Host tests for the Port 4 device model (protocol v1) */
#include "aoc_device.h"
#include "aoc_protocol.h"
#include "test_common.h"

int main(void) {
    AocDeviceState s = { 1, AOC_NOTE_NONE, 0, 0, 0 };
    uint8_t tx[1];
    uint8_t rx[16];
    int len;

    /* Status: non-standard type, no pak */
    tx[0] = AOC_CMD_STATUS;
    len = aoc_device_process(&s, tx, 1, rx, 3);
    CHECK(len == 3 && rx[0] == 0x4F && rx[1] == 0x43 && rx[2] == 0x00);
    tx[0] = AOC_CMD_RESET;
    CHECK(aoc_device_process(&s, tx, 1, rx, 3) == 3);

    /* Read buttons: zeros */
    tx[0] = AOC_CMD_READ_BUTTONS;
    len = aoc_device_process(&s, tx, 1, rx, 4);
    CHECK(len == 4 && rx[0] == 0 && rx[1] == 0 && rx[2] == 0 && rx[3] == 0);

    /* Identify */
    tx[0] = AOC_CMD_IDENTIFY;
    len = aoc_device_process(&s, tx, 1, rx, 6);
    CHECK(len == 6 && rx[0] == 'O' && rx[1] == 'C' && rx[2] == 'A' && rx[3] == 'R');
    CHECK(rx[4] == AOC_PROTOCOL_VERSION && (rx[5] & AOC_CAP_CONFIDENCE));

    /* Valid NO_NOTE */
    tx[0] = AOC_CMD_GET_STATE;
    len = aoc_device_process(&s, tx, 1, rx, 4);
    CHECK(len == 4 && rx[0] == AOC_NOTE_NONE && rx[3] == (uint8_t)(rx[0] ^ rx[1] ^ rx[2] ^ AOC_STATE_CHECK_XOR));

    /* Each note */
    for (s.note = AOC_NOTE_A; s.note <= AOC_NOTE_C_UP; s.note++) {
        s.confidence = 100 + s.note;
        len = aoc_device_process(&s, tx, 1, rx, 4);
        CHECK(len == 4 && rx[0] == s.note && rx[1] == s.confidence);
        CHECK(rx[3] == aoc_state_check(rx[0], rx[1], rx[2]));
    }

    /* Invalid internal note is reported as none */
    s.note = 42;
    aoc_device_process(&s, tx, 1, rx, 4);
    CHECK(rx[0] == AOC_NOTE_NONE && rx[1] == 0);

    /* Unknown commands (e.g. mempak read) get no response */
    tx[0] = 0x02;
    CHECK(aoc_device_process(&s, tx, 1, rx, 33) == AOC_NO_RESPONSE);
    tx[0] = 0x4D;
    CHECK(aoc_device_process(&s, tx, 1, rx, 4) == AOC_NO_RESPONSE);

    /* Disconnected: nothing answers */
    s.connected = 0;
    tx[0] = AOC_CMD_IDENTIFY;
    CHECK(aoc_device_process(&s, tx, 1, rx, 6) == AOC_NO_RESPONSE);
    tx[0] = AOC_CMD_STATUS;
    CHECK(aoc_device_process(&s, tx, 1, rx, 3) == AOC_NO_RESPONSE);

    /* IDENTIFY advertises SET_TRANSPOSE */
    s.connected = 1;
    tx[0] = AOC_CMD_IDENTIFY;
    aoc_device_process(&s, tx, 1, rx, 6);
    CHECK(rx[5] == (AOC_CAP_CONFIDENCE | AOC_CAP_TRANSPOSE));

    /* SET_TRANSPOSE stores, echoes and clamps; RESET clears it; STATUS keeps it */
    {
        uint8_t tt[2] = { AOC_CMD_SET_TRANSPOSE, (uint8_t)(int8_t)-12 };

        len = aoc_device_process(&s, tt, 2, rx, 1);
        CHECK(len == 1 && (int8_t)rx[0] == -12 && s.romTranspose == -12);
        tt[1] = 100;
        aoc_device_process(&s, tt, 2, rx, 1);
        CHECK((int8_t)rx[0] == AOC_TRANSPOSE_LIMIT && s.romTranspose == AOC_TRANSPOSE_LIMIT);
        CHECK(aoc_device_process(&s, tt, 1, rx, 1) == AOC_NO_RESPONSE); /* missing argument */
        tx[0] = AOC_CMD_STATUS;
        aoc_device_process(&s, tx, 1, rx, 3);
        CHECK(s.romTranspose == AOC_TRANSPOSE_LIMIT);
        tx[0] = AOC_CMD_RESET;
        aoc_device_process(&s, tx, 1, rx, 3);
        CHECK(s.romTranspose == 0);
    }

    /* Smaller rx buffer than the natural reply: reports natural length, writes only rxCap */
    s.connected = 1;
    rx[2] = 0xEE;
    tx[0] = AOC_CMD_IDENTIFY;
    CHECK(aoc_device_process(&s, tx, 1, rx, 2) == 6 && rx[2] == 0xEE);

    return TEST_SUMMARY();
}
