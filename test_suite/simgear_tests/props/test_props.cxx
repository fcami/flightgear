/*
 * SPDX-FileCopyrightText: 2016 Edward d'Auvergne
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "test_props.hxx"

#include <sstream>

#include <simgear/props/props_io.hxx>
#include <simgear/structure/SGSourceLocation.hxx>

// Set up function for each test.
void SimgearPropsTests::setUp()
{
    // Create a property tree.
    tree = new SGPropertyNode;
}


// Clean up after each test.
void SimgearPropsTests::tearDown()
{
    // Delete the tree (avoiding the memory leak).
    delete tree;
}


// Test property aliasing, to catch possible memory leaks.
void SimgearPropsTests::testAliasLeak()
{
    // Declarations.
    SGPropertyNode *alias;

    // Create a new node.
    tree->getNode("test-node", true);

    // Aliased node.
    alias = tree->getNode("test-alias", true);
    alias->alias("test-node", false);
}


void SimgearPropsTests::testDoubleAlias()
{
    // Declarations.
    SGPropertyNode* alias;

    // Create a new node.
    tree->getNode("test-node", true);
    tree->getNode("another-node", true);

    // Aliased node.
    alias = tree->getNode("test-alias", true);

    CPPUNIT_ASSERT(alias->alias("test-node", false));
    CPPUNIT_ASSERT(!alias->alias("another-node", false));
}


void SimgearPropsTests::testPropsCopyIf()
{
// dummy property tree structure
    tree->setIntValue("a/a/a", 42);
    tree->setStringValue("a/a/b", "foo");
    tree->setIntValue("a/a/c[2]", 99);
    tree->setIntValue("a/a/d", 1);
    tree->setIntValue("a/b/a[0]", 50);
    tree->setIntValue("a/b/a[1]", 100);
    tree->setStringValue("a/b/b", "foo");

    SGPropertyNode_ptr destA(new SGPropertyNode);

    copyPropertiesIf(tree, destA, [](const SGPropertyNode* src) {
        // always copy non-leaf nodes
        if (src->nChildren() > 0)
            return true;

        return (src->getType() == simgear::props::INT) &&
            src->getIntValue() > 50;
    });

    CPPUNIT_ASSERT_EQUAL(1, destA->getNode("a/a")->nChildren()); // only 99
    CPPUNIT_ASSERT_EQUAL(99, destA->getIntValue("a/a/c[2]"));
    CPPUNIT_ASSERT_EQUAL(1, destA->getNode("a/b")->nChildren()); // only 100
    CPPUNIT_ASSERT_EQUAL(100, destA->getIntValue("a/b/a[1]"));
}


// Test that nodes parsed from XML via readProperties() carry a valid
// SGSourceLocation.  The parser records a location for:
//   - every non-leaf (parent) node, and
//   - string/unspecified leaf nodes whose value spans more than one line.
// Single-line leaf nodes (any type) must NOT receive a location.
void SimgearPropsTests::testPropsXMLSourceLocation()
{
    // Line numbers in the comment match the 1-based line position inside the
    // string so that the CPPUNIT_ASSERT_EQUAL checks below are easy to verify.
    const std::string xml =
        "<?xml version=\"1.0\"?>\n"                  // line 1
        "<PropertyList>\n"                           // line 2
        "  <parent>\n"                               // line 3
        "    <multiline type=\"string\">first\n"     // line 4
        "second</multiline>\n"                       // line 5
        "    <plain type=\"string\">hello</plain>\n" // line 6
        "    <count type=\"int\">42</count>\n"       // line 7
        "  </parent>\n"                              // line 8
        "</PropertyList>\n";                         // line 9

    SGPropertyNode_ptr root(new SGPropertyNode);
    const std::string xmlPath = "/test/props.xml";
    std::istringstream iss(xml);
    readProperties(iss, root, xmlPath);

    // Non-leaf 'parent' node must have a valid source location pointing to
    // the opening tag on line 3.
    SGPropertyNode* parent = root->getNode("parent");
    CPPUNIT_ASSERT(parent != nullptr);
    const SGSourceLocation parentLoc = parent->getLocation();
    CPPUNIT_ASSERT(parentLoc.isValid());
    CPPUNIT_ASSERT_EQUAL(xmlPath, std::string(parentLoc.getPath()));
    CPPUNIT_ASSERT_EQUAL(3, parentLoc.getLine());

    // A string property whose value contains a newline must also carry a
    // valid source location pointing to its opening tag on line 4.
    SGPropertyNode* multiline = root->getNode("parent/multiline");
    CPPUNIT_ASSERT(multiline != nullptr);
    const SGSourceLocation mlLoc = multiline->getLocation();
    CPPUNIT_ASSERT(mlLoc.isValid());
    CPPUNIT_ASSERT_EQUAL(xmlPath, std::string(mlLoc.getPath()));
    CPPUNIT_ASSERT_EQUAL(4, mlLoc.getLine());

    // Single-line string and int leaf nodes must NOT have a location set.
    SGPropertyNode* plain = root->getNode("parent/plain");
    CPPUNIT_ASSERT(plain != nullptr);
    CPPUNIT_ASSERT(!plain->getLocation().isValid());

    SGPropertyNode* count = root->getNode("parent/count");
    CPPUNIT_ASSERT(count != nullptr);
    CPPUNIT_ASSERT(!count->getLocation().isValid());
}
