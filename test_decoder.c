#include "decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

static void assert_test(int condition, const char *test_name,
                        const char *message) {
  if (condition) {
    printf("  PASS: %s\n", test_name);
    tests_passed++;
  } else {
    printf("  FAIL: %s - %s\n", test_name, message);
    tests_failed++;
  }
}

void test_valid_messages() {
  printf("\n--- Valid Message Tests ---\n");

  {
    char buf[] = "0|05|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 10 && msg.version == 0 && msg.length == 5 &&
                strcmp(msg.type, "WAIT") == 0 && msg.field_count == 0 &&
                msg.error_code == 0);

    assert_test(pass, "valid_WAIT", "Should parse WAIT with 0 fields");
  }

  {
    char buf[] = "0|11|OPEN|Alice|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 16 && msg.version == 0 && msg.length == 11 &&
                strcmp(msg.type, "OPEN") == 0 && msg.field_count == 1 &&
                msg.fields[0] != NULL && strcmp(msg.fields[0], "Alice") == 0 &&
                msg.error_code == 0);

    assert_test(pass, "valid_OPEN_simple",
                "Should parse OPEN with simple name");
  }

  {
    char buf[] = "0|19|OPEN|Alice Johnson|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass =
        (result == 24 && msg.length == 19 && strcmp(msg.type, "OPEN") == 0 &&
         msg.field_count == 1 && strcmp(msg.fields[0], "Alice Johnson") == 0 &&
         msg.error_code == 0);

    assert_test(pass, "valid_OPEN_spaces",
                "Should parse OPEN with spaces in name");
  }

  {
    char buf[] = "0|13|NAME|1|Alice|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass =
        (result == 18 && msg.length == 13 && strcmp(msg.type, "NAME") == 0 &&
         msg.field_count == 2 && strcmp(msg.fields[0], "1") == 0 &&
         strcmp(msg.fields[1], "Alice") == 0 && msg.error_code == 0);

    assert_test(pass, "valid_NAME", "Should parse NAME with 2 fields");
  }

  {
    char buf[] = "0|17|PLAY|1|1 3 5 7 9|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass =
        (result == 22 && msg.length == 17 && strcmp(msg.type, "PLAY") == 0 &&
         msg.field_count == 2 && strcmp(msg.fields[0], "1") == 0 &&
         strcmp(msg.fields[1], "1 3 5 7 9") == 0 && msg.error_code == 0);

    assert_test(pass, "valid_PLAY", "Should parse PLAY with 2 fields");
  }

  {
    char buf[] = "0|09|MOVE|2|3|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass =
        (result == 14 && msg.length == 9 && strcmp(msg.type, "MOVE") == 0 &&
         msg.field_count == 2 && strcmp(msg.fields[0], "2") == 0 &&
         strcmp(msg.fields[1], "3") == 0 && msg.error_code == 0);

    assert_test(pass, "valid_MOVE", "Should parse MOVE with 2 fields");
  }

  {
    char buf[] = "0|18|OVER|1|1 3 5 7 9||";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass =
        (result == 23 && msg.length == 18 && strcmp(msg.type, "OVER") == 0 &&
         msg.field_count == 3 && strcmp(msg.fields[0], "1") == 0 &&
         strcmp(msg.fields[1], "1 3 5 7 9") == 0 &&
         strcmp(msg.fields[2], "") == 0 && msg.error_code == 0);

    assert_test(pass, "valid_OVER_empty_forfeit",
                "Should parse OVER with empty forfeit");
  }

  {
    char buf[] = "0|25|OVER|2|1 3 5 7 9|Forfeit|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass =
        (result == 30 && msg.length == 25 && strcmp(msg.type, "OVER") == 0 &&
         msg.field_count == 3 && strcmp(msg.fields[0], "2") == 0 &&
         strcmp(msg.fields[1], "1 3 5 7 9") == 0 &&
         strcmp(msg.fields[2], "Forfeit") == 0 && msg.error_code == 0);

    assert_test(pass, "valid_OVER_forfeit",
                "Should parse OVER with Forfeit string");
  }

  {
    char buf[] = "0|18|FAIL|Invalid move|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass =
        (result == 23 && msg.length == 18 && strcmp(msg.type, "FAIL") == 0 &&
         msg.field_count == 1 && strcmp(msg.fields[0], "Invalid move") == 0 &&
         msg.error_code == 0);

    assert_test(pass, "valid_FAIL", "Should parse FAIL with error message");
  }
}

void test_length_validation() {
  printf("\n--- Length Validation Tests ---\n");

  /* Stated length too short for content */
  {
    char buf[] = "0|03|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "length_too_short",
                "Should reject when stated length too short");
  }

  /* Stated length exceeds buffer */
  {
    char buf[] = "0|50|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 0);
    assert_test(pass, "length_exceeds_buffer",
                "Should return 0 for incomplete message");
  }

  {
    char buf[] = "0|5|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "length_one_digit", "Should reject single-digit length");
  }

  /* Length < 5 */
  {
    char buf[] = "0|04|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "length_below_minimum", "Should reject length < 5");
  }

  /* Length field has non-digits */
  {
    char buf[] = "0|ab|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "length_non_digits", "Should reject non-digit length");
  }
}

void test_incomplete_messages() {
  printf("\n--- Incomplete Message Tests ---\n");

  {
    char buf[] = "0";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 0);
    assert_test(pass, "incomplete_version_only",
                "Should return 0 for version only");
  }

  /* Version and partial length */
  {
    char buf[] = "0|1";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 0);
    assert_test(pass, "incomplete_partial_length",
                "Should return 0 for partial length");
  }

  {
    char buf[] = "0|11|OPEN";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 0);
    assert_test(pass, "incomplete_header_only",
                "Should return 0 for header only");
  }

  {
    char buf[] = "0|11|OPEN|Ali";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 0);
    assert_test(pass, "incomplete_partial_field",
                "Should return 0 for partial field");
  }

  {
    char buf[] = "0|11|OPEN|Alice";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 0);
    assert_test(pass, "incomplete_no_final_delim",
                "Should return 0 for missing final |");
  }

  {
    char buf[] = "";
    Message msg = {0};
    int result = parse_message(buf, 0, &msg);

    int pass = (result == 0);
    assert_test(pass, "incomplete_empty", "Should return 0 for empty buffer");
  }
}

void test_invalid_format() {
  printf("\n--- Invalid Format Tests ---\n");

  {
    char buf[] = "0|05|BLAH|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "invalid_unknown_type",
                "Should reject unknown message type");
  }

  {
    char buf[] = "1|05|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "invalid_version", "Should reject version != 0");
  }

  {
    char buf[] = "a|05|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "invalid_version_non_digit",
                "Should reject non-digit version");
  }

  {
    char buf[] = "005|WAIT|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "invalid_no_version_delim",
                "Should reject missing version delimiter");
  }

  {
    char buf[] = "0|05|OPEN|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "invalid_too_few_fields", "Should reject too few fields");
  }

  {
    char buf[] = "0|16|WAIT|extra|data|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "invalid_too_many_fields",
                "Should reject too many fields");
  }

  {
    char buf[] = "0|05|WAI||";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_INVALID);
    assert_test(pass, "invalid_short_type", "Should reject type < 4 chars");
  }
}

void test_edge_cases() {
  printf("\n--- Edge Case Tests ---\n");

  {
    char buf[] = "0|06|OPEN||";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 11 && msg.field_count == 1 && msg.fields[0] != NULL &&
                strcmp(msg.fields[0], "") == 0 && msg.error_code == 0);

    assert_test(pass, "edge_empty_name", "Should accept empty player name");
  }

  {
    char name[73];
    memset(name, 'A', 72);
    name[72] = '\0';

    char buf[100];
    snprintf(buf, sizeof(buf), "0|78|OPEN|%s|", name);
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 83 && msg.field_count == 1 &&
                strlen(msg.fields[0]) == 72 && msg.error_code == 0);

    assert_test(pass, "edge_max_name_72", "Should accept 72-char name");
  }

  {
    char name[74];
    memset(name, 'B', 73);
    name[73] = '\0';

    char buf[100];
    snprintf(buf, sizeof(buf), "0|79|OPEN|%s|", name);
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == -1 && msg.error_code == ERR_LONG_NAME);

    assert_test(pass, "edge_name_too_long",
                "Should reject 73-char name with ERR_LONG_NAME");
  }

  {
    char buf[] = "0|08|PLAY|1||";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result == 13 && msg.field_count == 2 &&
                strcmp(msg.fields[0], "1") == 0 &&
                strcmp(msg.fields[1], "") == 0 && msg.error_code == 0);

    assert_test(pass, "edge_empty_board", "Should accept empty board state");
  }

  {
    char buf[] = "0|05|WAIT|0|11|OPEN|Alice|";
    Message msg = {0};

    int result1 = parse_message(buf, strlen(buf), &msg);
    int pass1 = (result1 == 10 && strcmp(msg.type, "WAIT") == 0);

    Message msg2 = {0};
    int result2 = parse_message(buf + result1, strlen(buf) - result1, &msg2);
    int pass2 = (result2 == 16 && strcmp(msg2.type, "OPEN") == 0);

    assert_test(pass1 && pass2, "edge_multiple_messages",
                "Should handle multiple messages in buffer");
  }

  {
    char buf[] = "0|17|OPEN|A@#$%^&*()|";
    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass =
        (result == 22 && msg.field_count == 1 &&
         strcmp(msg.fields[0], "A@#$%^&*()") == 0 && msg.error_code == 0);

    assert_test(pass, "edge_special_chars", "Should handle special characters");
  }
}

void test_encoder() {
  printf("\n--- Encoder Tests ---\n");

  {
    char buf[100];
    int n = encode_message(buf, sizeof(buf), "WAIT");

    int pass = (n == 10 && strcmp(buf, "0|05|WAIT|") == 0);
    assert_test(pass, "encode_WAIT", "Should encode WAIT correctly");
  }

  {
    char buf[100];
    int n = encode_message(buf, sizeof(buf), "OPEN", "Alice");

    int pass = (n == 16 && strcmp(buf, "0|11|OPEN|Alice|") == 0);
    assert_test(pass, "encode_OPEN", "Should encode OPEN correctly");
  }

  {
    char buf[100];
    int n = encode_message(buf, sizeof(buf), "NAME", "1", "Alice");

    int pass = (n == 18 && strcmp(buf, "0|13|NAME|1|Alice|") == 0);
    assert_test(pass, "encode_NAME", "Should encode NAME correctly");
  }

  {
    char buf[100];
    int n = encode_message(buf, sizeof(buf), "PLAY", "1", "1 3 5 7 9");

    int pass = (n == 22 && strcmp(buf, "0|17|PLAY|1|1 3 5 7 9|") == 0);
    assert_test(pass, "encode_PLAY", "Should encode PLAY correctly");
  }

  {
    char buf[100];
    int n = encode_message(buf, sizeof(buf), "OVER", "1", "0 0 0 0 0", "");

    int pass = (n == 23 && strcmp(buf, "0|18|OVER|1|0 0 0 0 0||") == 0);
    assert_test(pass, "encode_OVER_empty",
                "Should encode OVER with empty forfeit");
  }

  {
    char buf[100];
    int n =
        encode_message(buf, sizeof(buf), "OVER", "2", "0 0 0 0 0", "Forfeit");

    int pass = (n == 30 && strcmp(buf, "0|25|OVER|2|0 0 0 0 0|Forfeit|") == 0);
    assert_test(pass, "encode_OVER_forfeit", "Should encode OVER with Forfeit");
  }

  {
    char buf[100];
    int n = encode_message(buf, sizeof(buf), "FAIL", "10 Invalid");

    int pass = (n == 20 && strcmp(buf, "0|15|FAIL|10 Invalid|") == 0);
    assert_test(pass, "encode_FAIL", "Should encode FAIL correctly");
  }

  {
    char buf[100];
    int n = encode_message(buf, sizeof(buf), "BLAH");

    int pass = (n == -1);
    assert_test(pass, "encode_unknown_type", "Should fail for unknown type");
  }

  {
    char buf[5];
    int n = encode_message(buf, sizeof(buf), "WAIT");

    int pass = (n == -1);
    assert_test(pass, "encode_buffer_small", "Should fail if buffer too small");
  }

  {
    char buf[100];
    encode_message(buf, sizeof(buf), "OPEN", "TestUser");

    Message msg = {0};
    int result = parse_message(buf, strlen(buf), &msg);

    int pass = (result > 0 && strcmp(msg.type, "OPEN") == 0 &&
                strcmp(msg.fields[0], "TestUser") == 0);

    assert_test(pass, "encode_decode_roundtrip",
                "Round-trip should preserve data");
  }
}

void test_encode_fail() {
  printf("\n--- encode_fail() Tests ---\n");

  {
    char buf[100];
    int n = encode_fail(buf, sizeof(buf), ERR_INVALID);

    int pass = (n > 0 && strstr(buf, "FAIL") != NULL &&
                strstr(buf, "10 Invalid") != NULL);
    assert_test(pass, "encode_fail_invalid", "Should encode ERR_INVALID");
  }

  {
    char buf[100];
    int n = encode_fail(buf, sizeof(buf), ERR_LONG_NAME);

    int pass = (n > 0 && strstr(buf, "21 Long Name") != NULL);
    assert_test(pass, "encode_fail_long_name", "Should encode ERR_LONG_NAME");
  }

  {
    char buf[100];
    int n = encode_fail(buf, sizeof(buf), ERR_IMPATIENT);

    int pass = (n > 0 && strstr(buf, "31 Impatient") != NULL);
    assert_test(pass, "encode_fail_impatient", "Should encode ERR_IMPATIENT");
  }
}

int main() {
  printf("==============================================\n");
  printf("   NGP Message Decoder Test Suite\n");
  printf("==============================================\n");

  test_valid_messages();
  test_length_validation();
  test_incomplete_messages();
  test_invalid_format();
  test_edge_cases();
  test_encoder();
  test_encode_fail();

  printf("\n==============================================\n");
  printf("   Results: %d passed, %d failed\n", tests_passed, tests_failed);
  printf("==============================================\n");

  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
