import sys
from reflection.reflect import reflect

if __name__ == "__main__":
    if (len(sys.argv) < 2):
        print("Usage: " + sys.argv[0] + " <res_path> [config1] [config2] ...")
        exit(-1)
    print("White Bird Engine build script start.")
    reflect()
    print("White Bird Engine build script finish.")
