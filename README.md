# Osprey Timing EPICS Driver

## Dependencies

- [EPICS Base](https://github.com/epics-base/epics-base) >= 7.0.8.1
- [feed-core](https://github.com/osprey-dcs/feed-core)
- [autosave](https://github.com/epics-modules/autosave)
- [PVXS](https://github.com/epics-base/pvxs) (Optional)
  - Requires libevent2

# Building

```sh
apt-get update
apt-get install -y build-essential git libevent-dev libz-dev libreadline-dev python3 python-is-python3

git clone --branch 7.0 https://github.com/epics-base/epics-base
git clone --branch master https://github.com/epics-base/pvxs
git clone --branch master https://github.com/epics-modules/autosave
git clone --branch master https://github.com/osprey-dcs/feed-core
git clone --branch master https://github.com/osprey-dcs/timing-ioc

cat <<EOF > pvxs/configure/RELEASE.local
EPICS_BASE=\$(TOP)/../epics-base
EOF

cat <<EOF > feed-core/configure/RELEASE.local
EPICS_BASE=\$(TOP)/../epics-base
EOF

cat <<EOF > autosave/configure/RELEASE.local
EPICS_BASE=\$(TOP)/../epics-base
EOF

cat <<EOF > timing-ioc/configure/RELEASE.local
AUTOSAVE=\$(TOP)/../autosave
PVXS=\$(TOP)/../pvxs
FEED_CORE=\$(TOP)/../feed-core
EPICS_BASE=\$(TOP)/../epics-base
EOF

make -C epics-base
make -C pvxs
make -C autosave
make -C feed-core
make -C timing-ioc
```

## Running

```sh
cd timing-ioc/iocBoot/iocEVT/

EVT_IPADDR=1.2.3.4 ./st.cmd
```

## OPI Screens

Virtual control panel display files for [Phoebus](http://phoebus.org/)
are provided under `opi/`.

## Details

- [Boot control](documentation/boot.md)
- [Timing system reference](documentation/README.md)
