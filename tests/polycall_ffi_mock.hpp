#ifndef POLYCALL_FFI_MOCK_HPP
#define POLYCALL_FFI_MOCK_HPP

void polycall_ffi_mock_reset();
void polycall_ffi_mock_return_status(int status);
int polycall_ffi_mock_call_count();
int polycall_ffi_mock_last_run();
const char* polycall_ffi_mock_last_config();

#endif // POLYCALL_FFI_MOCK_HPP
