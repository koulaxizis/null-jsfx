# NULL JSFX Remote for Ableton Live: the control surface that lets the Android app
# Remote for DAWs (tab Ableton) run Live's mixer, transport and devices. See remote.py.
# NULL JSFX • https://nulljsfx.tech • MIT License
from __future__ import absolute_import

from .remote import Remote


def create_instance(c_instance):
    return Remote(c_instance)
