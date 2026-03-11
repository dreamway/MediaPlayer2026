# testing/pyobjc/core/ax_element.py
"""
Accessibility Element Wrapper for macOS UI Automation
"""

from ApplicationServices import (
    AXUIElementCopyAttributeValue,
    AXUIElementPerformAction,
    AXUIElementSetAttributeValue,
    kAXChildrenAttribute,
    kAXTitleAttribute,
    kAXValueAttribute,
    kAXRoleAttribute,
    kAXEnabledAttribute,
    kAXPressAction,
    kAXIncrementAction,
    kAXDecrementAction,
    kAXMinValueAttribute,
    kAXMaxValueAttribute,
)


class AXElementError(Exception):
    """Exception raised for AXElement operations."""
    pass


class AXElement:
    """
    Wrapper class for macOS Accessibility UI Elements.
    Provides methods to interact with UI elements through the Accessibility API.
    """

    def __init__(self, ax_element):
        """
        Initialize AXElement wrapper.

        Args:
            ax_element: The underlying AXUIElementRef
        """
        self._element = ax_element

    def get_title(self) -> str:
        """
        Get the title of the element.

        Returns:
            str: The element's title, or empty string if not available.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXTitleAttribute, None
        )
        if result == 0 and value is not None:
            return str(value)
        return ""

    def get_role(self) -> str:
        """
        Get the role of the element (e.g., 'AXButton', 'AXSlider').

        Returns:
            str: The element's role, or empty string if not available.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXRoleAttribute, None
        )
        if result == 0 and value is not None:
            return str(value)
        return ""

    def get_value(self) -> str:
        """
        Get the value of the element (e.g., slider value, text field content).

        Returns:
            str: The element's value, or empty string if not available.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXValueAttribute, None
        )
        if result == 0 and value is not None:
            return str(value)
        return ""

    def get_numeric_value(self) -> float:
        """
        Get the value as number (for sliders). Returns 0.0 if not a number.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXValueAttribute, None
        )
        if result == 0 and value is not None:
            try:
                return float(value)
            except (TypeError, ValueError):
                pass
        return 0.0

    def get_min_value(self) -> float:
        """Slider/range 最小值。"""
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXMinValueAttribute, None
        )
        if result == 0 and value is not None:
            try:
                return float(value)
            except (TypeError, ValueError):
                pass
        return 0.0

    def get_max_value(self) -> float:
        """Slider/range 最大值。"""
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXMaxValueAttribute, None
        )
        if result == 0 and value is not None:
            try:
                return float(value)
            except (TypeError, ValueError):
                pass
        return 0.0

    def set_value(self, value) -> bool:
        """
        Set the value of the element.

        Args:
            value: The value to set (string for text, number for sliders).

        Returns:
            bool: True if successful, False otherwise.
        """
        result = AXUIElementSetAttributeValue(
            self._element, kAXValueAttribute, value
        )
        return result == 0

    def is_enabled(self) -> bool:
        """
        Check if the element is enabled.

        Returns:
            bool: True if enabled, False otherwise.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXEnabledAttribute, None
        )
        if result == 0 and value is not None:
            return bool(value)
        return False

    def get_children(self) -> list:
        """
        Get all child elements of this element.

        Returns:
            list: List of AXElement objects representing children.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXChildrenAttribute, None
        )
        if result == 0 and value is not None:
            children = []
            for i in range(len(value)):
                children.append(AXElement(value[i]))
            return children
        return []

    def find_child_by_title(self, title: str) -> 'AXElement':
        """
        Find a child element by its title.

        Args:
            title: The title to search for.

        Returns:
            AXElement: The found element, or None if not found.
        """
        for child in self.get_children():
            if child.get_title() == title:
                return child
            # Recursively search in children
            found = child.find_child_by_title(title)
            if found is not None:
                return found
        return None

    def find_child_by_role(self, role: str) -> 'AXElement':
        """
        Find a child element by its role.

        Args:
            role: The role to search for (e.g., 'AXButton', 'AXSlider').

        Returns:
            AXElement: The found element, or None if not found.
        """
        for child in self.get_children():
            if child.get_role() == role:
                return child
            # Recursively search in children
            found = child.find_child_by_role(role)
            if found is not None:
                return found
        return None

    def find_all_by_role(self, role: str) -> list:
        """
        Find all descendant elements with the given role.

        Args:
            role: The role to search for (e.g., 'AXStaticText', 'AXSlider').

        Returns:
            list: List of AXElement objects.
        """
        result = []
        for child in self.get_children():
            if child.get_role() == role:
                result.append(child)
            result.extend(child.find_all_by_role(role))
        return result

    def click(self) -> bool:
        """
        Perform a click/press action on the element.

        Returns:
            bool: True if successful, False otherwise.
        """
        result = AXUIElementPerformAction(self._element, kAXPressAction)
        return result == 0

    def increment(self) -> bool:
        """
        Perform an increment action on the element (e.g., slider increment).

        Returns:
            bool: True if successful, False otherwise.
        """
        result = AXUIElementPerformAction(self._element, kAXIncrementAction)
        return result == 0

    def decrement(self) -> bool:
        """
        Perform a decrement action on the element (e.g., slider decrement).

        Returns:
            bool: True if successful, False otherwise.
        """
        result = AXUIElementPerformAction(self._element, kAXDecrementAction)
        return result == 0

    def __repr__(self):
        return f"AXElement(title='{self.get_title()}', role='{self.get_role()}')"