import pytest

from Tests.cam_utility import calibrate_from_full_on
from kewet_display import KewetDisplay


@pytest.fixture
def display():

    disp = KewetDisplay(port="COM9")  # Adjust port as necessary
    yield disp
    disp.close()

def cam_util(display):
    calibrate_from_full_on