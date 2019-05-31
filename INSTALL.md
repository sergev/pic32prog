# Installing pic32prog

## Checkout

After cloning the repository, check out the submodules:

```
git submodule update --init
```

## Dependencies

Make sure to gather all the dependencies: libusb-1.0, and libudev if you're using Linux.

## Build

Run the following command:

```
make
```

Afterwards, the file `pic32prog` will appear in the build directory.
