package main

import (
    "os"
	"fmt"

    "cpplus.cc/goself/tleveldb"
)

func usage() {

    fmt.Println("storebackup")
    fmt.Println("\t(backup and restore leveldb tools)")
    fmt.Println("")
    fmt.Println("\t back    dbpath storefile")
    fmt.Println("\t restore dbpath storefile")
    fmt.Println("\t dump    dbpath")
    fmt.Println("")

    os.Exit(-1)
}

func main() {

    if len(os.Args) == 3 {
        if os.Args[1] != "dump" {
            usage()
        }

        dbpath := os.Args[2]
        _, err := tleveldb.BrowseLevelDB(dbpath)
        if err != nil {
            fmt.Print("error when BrowseLevelDB:", err)
        }

    } else if len(os.Args) == 4 {
        if os.Args[1] != "back" && os.Args[1] != "restore" {
            usage()
        }

        cmd := os.Args[1]
        dbpath := os.Args[2]
        file := os.Args[3]

        if cmd == "back" {
            cnt, err := tleveldb.BackupLevelDB(dbpath, file)
            if err != nil {
                fmt.Print("error when BackupLevelDB:", cnt, err)
                os.Exit(-1)
            }
            fmt.Print("BackupLevelDB OK: ", cnt)
        } else if cmd == "restore" {
            cnt, err := tleveldb.RestoreLevelDB(dbpath, file)
            if err != nil {
                fmt.Print("error when RestoreLevelDB:", cnt, err)
                os.Exit(-1)
            }
            fmt.Print("RestoreLevelDB OK: ", cnt)
        }

    } else {
        usage()
    }

}
