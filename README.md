# my-sma-bluetooth

## Prerequisites

Create a file `.my-smatool.conf` with your personal sma settings in same folder as the `docker-compose.yml` file.  You can use file [smatool.conf.new](./sma-bluetooth/src/smatool.conf.new) as a template for this file.

## Installation

1. run `make all`
2. Copy your local `.my-smatool.conf` to docker volume `my-sma-config` which is mounted in your container.

```shell
make copy_config
```

3. Attach a shell to docker container
4. In shell enter following command which will create the `smatool` database (you can get more output by adding options `-v` or `-d`)

 ```shell
# add option -v or -d if you want a more verbose/debug output
cd /etc; smatool --INSTALL -c /my-sma-config/smatool.conf
```

## Run smatool once

1. Attach a shell to docker container
2 In shell enter following command

 ```shell
 # add option -v or -d if you want a more verbose/debug output
cd /etc; smatool -c /my-sma-config/smatool.conf
```

## Check contents of smatool database

Using DBeaver you can create a connection to the mariadb (`Port=3306` which is the default Port for mysql db and mariadb).
