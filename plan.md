
# Phase 3
1. When on the screensaver screen where we are showing the bus times after the devices loads the screen seems to be flickering please explore what could be wrong here.
2. add a second option to the bus screen to "refresh" and check times again

# Code Issues Analysis

## Critical Issues

### 1. Screen Flickering on Standby (EXISTING ISSUE)
**File:** `DisplayManager.cpp:187` (drawStandbyScreen)
**Issue:** Screen flicker likely caused by unnecessary redraws in standby mode. The drawStandbyScreen method returns early if minutes match, but there might be other components causing redraws.
**Root Cause:** Potential conflict with updateBaseZone() calls or frequent bus data updates triggering screen refreshes.

### 2. Memory Leaks - Dynamic Allocation Never Freed
**Files:** `main.cpp:29-31`
**Issue:** Objects created with `new` are never deleted:
- `sensor = new DistanceSensor(...)`
- `displayManager = new DisplayManager(...)`
- `stateManager = new StateManager(...)`
**Impact:** Memory leaks that accumulate over device runtime.

### 3. Stack Overflow Risk in Network Task
**File:** `NetworkManager.cpp:16-17`
**Issue:** Network task allocated with 8192 bytes stack, but HTTP operations can be stack-heavy. No monitoring for stack usage.
**Risk:** Random crashes during network operations.

## Moderate Issues

### 4. Infinite Loop Risk in JSON Parsing
**File:** `BusClient.cpp:67-95`
**Issue:** `while (stream.available())` loop can become infinite if stream doesn't properly terminate or contains unexpected data.
**Solution:** Add iteration limit or timeout mechanism.

### 5. Hardcoded Pin Definitions
**File:** `main.cpp:9-18`
**Issue:** Pin assignments are hardcoded in main.cpp instead of Config.h, making hardware changes difficult.
**Impact:** Poor maintainability for different board configurations.

### 6. No WiFi Reconnection Logic
**File:** `main.cpp:42-54`
**Issue:** WiFi connection is attempted once at startup. If connection fails later, there's no automatic reconnection.
**Impact:** Device becomes non-functional after WiFi dropout.

### 7. Unsafe String Operations
**File:** `BusClient.cpp:36`, `WledClient.cpp:20`
**Issue:** `snprintf` and string concatenation without proper bounds checking.
**Risk:** Buffer overflow potential.

## Minor Issues

### 8. Inconsistent Error Handling
**Files:** Multiple
**Issue:** Some functions return error codes (-1, -2, -3) while others use boolean or void returns.
**Impact:** Inconsistent error propagation and handling.

### 9. Magic Numbers Throughout Code
**Files:** Multiple (e.g., `StateManager.cpp:87`, timeouts, delays)
**Issue:** Hardcoded values like 1000ms, 3000ms, 400ms should be defined constants.
**Impact:** Difficult to tune and maintain.

### 10. Missing Input Validation
**Files:** Various
**Issue:** Functions don't validate input parameters (null pointers, range checks).
**Risk:** Potential crashes with invalid inputs.

### 11. Inefficient Redraw Logic
**File:** `DisplayManager.cpp`
**Issue:** Multiple methods check for changes but some comparisons might miss edge cases.
**Impact:** Unnecessary redraws or missed updates.

### 12. Resource Management Issues
**Files:** `BusClient.cpp:19-55`, `WledClient.cpp:19-38`
**Issue:** HTTPClient objects created/destroyed frequently without connection pooling.
**Impact:** Network overhead and potential memory fragmentation.

## Recommendations

1. **Fix screen flickering** by adding better redraw control and debugging display updates
2. **Implement proper memory management** with destructors or smart pointers
3. **Add WiFi reconnection logic** with connection monitoring
4. **Move all constants to Config.h** for better maintainability
5. **Add proper error handling** throughout the codebase
6. **Implement watchdog timer** for system reliability
7. **Add bounds checking** for all string operations
8. **Create connection pooling** for HTTP clients
9. **Add stack usage monitoring** for the network task
10. **Implement proper JSON parsing** with error bounds