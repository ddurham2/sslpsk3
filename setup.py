# Copyright 2017 David R. Bild
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License

from setuptools import setup, Extension

import os, shutil, sys, platform

_sslpsk3 = Extension('sslpsk3._sslpsk3',
                    sources=['sslpsk3/_sslpsk3.c'],
                    libraries=["ssl", ],
                    )

setup(
    name='sslpsk3',
    version='1.0.0',
    description='Adds TLS-PSK support to the Python ssl package',
    author='Sidney Kuyateh',
    author_email='sidneyjohn23@kuyateh.eu',
    license="Apache 2.0",
    url='https://github.com/autinerd/sslpsk3',
    keywords=['ssl', 'tls', 'psk', 'tls-psk', 'preshared key'],
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: Implementation :: CPython',
        'Operating System :: POSIX',
        'Operating System :: Unix',
        'Operating System :: MacOS',
        'Operating System :: Microsoft'
    ],
    packages=['sslpsk3', 'sslpsk3.test'],
    ext_modules=[_sslpsk3],
    #package_data={'': ['%s.dll' % lib for lib in DLL_NAMES]},
    test_suite='sslpsk3.test',
    zip_safe=False
)
