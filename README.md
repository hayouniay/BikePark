# Bike Parking (Qt 6 · C++26 · CMake · VS Code)

A redesign of the original Bike Booking Qt app. It manages bike slots (16 at first, A–P, and you can add or delete them): book a free slot,
release a booked one, and see live counts. Each booking records who made it and when.

## Requirements (Ubuntu 24.04 or WSL2 Ubuntu 24.04)

```bash
sudo apt update
sudo apt install -y build-essential ninja-build gdb pipx clang-format clang-tidy clangd gcc-14 g++-14
pipx install cmake            # CMake 3.25 or newer; apt's version may be too old for C++26
pipx ensurepath               # then open a new shell
```

**Qt 6.6 or newer** (6.8 LTS recommended). Install it with aqtinstall (the `desktop` package includes
the Test module used by the tests):

```bash
pipx install aqtinstall
aqt install-qt linux desktop 6.8.3 linux_gcc_64 -O ~/Qt
```

This installs to `~/Qt/6.8.3/gcc_64`, which matches `CMakePresets.json`. If you use a different
version or location, edit `CMAKE_PREFIX_PATH` in that file. The Ubuntu package `qt6-base-dev`
also works. Install `libqt6sql6-sqlite` alongside it so the SQLite driver is available.

## Build and run in VS Code

1. Open the folder: `code .` from the WSL terminal (install the *WSL* extension on Windows).
2. Accept the recommended extensions (clangd, CMake Tools, C/C++).
3. `Ctrl+Shift+P` → **Tasks: Run Task**, then pick a task.

Press **F5** to debug with the *Debug BikeParking* launch configuration (it runs *Build · debug* first). Files also format on save
through clangd.

From the terminal the same steps are:

```bash
cmake --preset debug && cmake --build --preset debug
./build/debug/BikeParking                          # uses the default data folder
./build/debug/BikeParking --database /path/to.sqlite   # uses a specific database file
ctest --test-dir build/debug --output-on-failure   # runs the tests
```

To build without the tests, configure with `-DBIKEPARKING_BUILD_TESTS=OFF`.

## Accounts

- **First run:** the app asks you to create the administrator account before anything else.
- **Sign in:** every start shows a sign-in window. Usernames are not case-sensitive. The Account menu
  has Sign out and, for administrators, New user account.
- **Passwords:** stored as salted PBKDF2-HMAC-SHA256 hashes (120,000 iterations), never in plain text.
  They must be at least 8 characters.
- **Roles:** members book and release their own slots. Administrators can do everything members can,
  plus add or delete slots, release any booking, and create accounts. Only the first account is
  created automatically as an administrator; others are members unless an administrator ticks the
  Administrator box.
- **Bookings made before accounts existed** have no owner, so only an administrator can release them.

## Sharing slots between users

Put the database on a shared folder and start every copy of the app with `--database` pointing at
it. The first run copies the bundled bookings into that file. SQLite is designed for one machine:
a network share can lock or corrupt the file if several computers write at the same time. For a
busy shared setup, a small server in front of the database is the safer choice.

## Project layout

```
src/
  main.cpp                    startup, command-line option, data-dir setup, first-run database copy
  core/Slot.h                 plain data type (slot, owner, booking time)
  core/ParkingRepository.*    SQLite access, expiry and migration, returns std::expected
  ui/SlotListModel.*          QAbstractListModel over the slots, tooltips and accessible text
  ui/SlotDelegate.*           paints each slot card (cached, HiDPI-aware photos)
  ui/MainWindow.*             layout, actions, keyboard handling, auto-refresh
  ui/Icons.*                  vector icons for buttons, the window and the placeholder
  ui/Theme.*                  card colours for light and dark mode
tests/
  tst_repository.cpp          QtTest suite for the repository
tools/
  make_bike_images.py         generates the slot photos (Pillow)
resources/                    images, stylesheet (style/style.qss), bundled database
```

## Known limits

- There is no way yet to change a password or reset a forgotten one. An administrator can create a new account, but cannot change an existing password.
- Accounts live in the same database file as the bookings. Anyone with write access to that file can edit it, so use a shared file only on a trusted network. Passwords stay hashed.
- There is no account lockout after repeated wrong passwords.
- Slot photos are generated illustrations, not photographs. To use photos, replace `resources/images/bike1..6.jpg` with images of the same size and ratio (960×608 or similar).
- Only six images exist, so with more than six slots the pictures repeat.
- Slot names are limited to 6 letters or digits.

## Ideas for next steps

- An administrator view with booking history.
- Per-user accounts, if bookings must be trustworthy.
