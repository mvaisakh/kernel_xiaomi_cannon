#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/backlight.h>
#include <linux/pwm.h>
#include <linux/leds.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/bitops.h>
#include <linux/mfd/ktd3136.h>
#include <linux/device.h>
#include <linux/platform_device.h>

struct ktd3137_chip *bkl_chip;
int ktd_hbm_mode;

#define KTD_DEBUG

 #ifdef KTD_DEBUG
#define LOG_DBG(fmt, args...) printk(KERN_INFO "[ktd]"fmt"\n", ##args)
#endif

int ktd_Exponential[2048] = {0, 10, 12, 14, 16, 20, 55, 80, 105, 131, 144, 150, 162, 171, 180, 198, 224, 252, 278, 304,  330, 356, 397,\
				423, 448, 473, 498, 522, 545, 568, 585, 600, 618, 636, 654, 672, 690, 715, 730, 748, 765, 790, 815, 835, 848, 854, 860, 866, 872,\
				878, 884, 890, 896, 901, 906, 911, 916, 921, 926, 931, 936, 941, 946, 951, 956, 960, 964, 968, 972, 976, 980, 984, 988, 992, 996,\
				998, 1001, 1004, 1007, 1010, 1013, 1016, 1019, 1022, 1025, 1028, 1031, 1034, 1037, 1040, 1043, 1046, 1049, 1052, 1055, 1058,\
				1061, 1064, 1067, 1070, 1073, 1076, 1079, 1082, 1085, 1088, 1091, 1094, 1097, 1100, 1103, 1106, 1109, 1112, 1115, 1118, 1121,\
				1124, 1127, 1130, 1133, 1136, 1139, 1142, 1145, 1148, 1151, 1154, 1157, 1160, 1163, 1166, 1169, 1172, 1175, 1178, 1181, 1184,\
				1187, 1190, 1193, 1196, 1199, 1202, 1205, 1208, 1211, 1214, 1217, 1220, 1223, 1226, 1229, 1232, 1235, 1238, 1241, 1244, 1247,\
				1250, 1253, 1256, 1259, 1262, 1265, 1268, 1271, 1274, 1277, 1280, 1283, 1286, 1289, 1292, 1295, 1298, 1301, 1304, 1307, 1310,\
				1310, 1312, 1314, 1316, 1318, 1320, 1322, 1324, 1326, 1328, 1330, 1332, 1334, 1336, 1338, 1340, 1342, 1344, 1346, 1348, 1350,\
				1352, 1353, 1354, 1355, 1356, 1357, 1358, 1359, 1360, 1361, 1362, 1363, 1364, 1365, 1366, 1367, 1368, 1369, 1370, 1371, 1372,\
				1373, 1374, 1375, 1376, 1377, 1378, 1379, 1380, 1381, 1382, 1383, 1384, 1385, 1386, 1387, 1388, 1389, 1390, 1391, 1392, 1393,\
				1394, 1395, 1396, 1397, 1398, 1399, 1400, 1401, 1402, 1403, 1404, 1405, 1406, 1407, 1408, 1409, 1410, 1411, 1412, 1413, 1414,\
				1415, 1416, 1417, 1418, 1419, 1420, 1421, 1422, 1423, 1424, 1425, 1426, 1427, 1428, 1429, 1430, 1431, 1432, 1433, 1434, 1435,\
				1436, 1437, 1438, 1439, 1440, 1441, 1442, 1443, 1444, 1445, 1446, 1447, 1448, 1449, 1450, 1451, 1452, 1453, 1454, 1455, 1456,\
				1457, 1458, 1459, 1460, 1461, 1462, 1463, 1464, 1465, 1466, 1467, 1468, 1469, 1470, 1471, 1472, 1473, 1474, 1475, 1476, 1477,\
				1478, 1479, 1480, 1481, 1482, 1483, 1484, 1485, 1486, 1487, 1488, 1489, 1490, 1491, 1492, 1493, 1494, 1495, 1496, 1497, 1498,\
				1499, 1500, 1501, 1502, 1503, 1504, 1505, 1506, 1507, 1508, 1509, 1510, 1511, 1512, 1513, 1514, 1515, 1516, 1517, 1518, 1519,\
				1520, 1520, 1521, 1521, 1522, 1522, 1523, 1523, 1524, 1524, 1525, 1525, 1526, 1526, 1527, 1527, 1528, 1528, 1529, 1529, 1530,\
				1531, 1531, 1532, 1532, 1533, 1533, 1534, 1534, 1535, 1535, 1536, 1536, 1537, 1537, 1538, 1538, 1539, 1539, 1540, 1540, 1541,\
				1542, 1542, 1542, 1542, 1543, 1543, 1543, 1543, 1544, 1544, 1544, 1544, 1545, 1545, 1545, 1545, 1546, 1546, 1546, 1546, 1547,\
				1547, 1547, 1547, 1547, 1548, 1548, 1548, 1548, 1549, 1549, 1549, 1549, 1550, 1550, 1550, 1550, 1551, 1551, 1551, 1551, 1552,\
				1552, 1552, 1552, 1552, 1553, 1553, 1553, 1553, 1554, 1554, 1554, 1554, 1555, 1555, 1555, 1555, 1556, 1556, 1556, 1556, 1557,\
				1557, 1557, 1557, 1557, 1558, 1558, 1558, 1558, 1559, 1559, 1559, 1559, 1560, 1560, 1560, 1560, 1561, 1561, 1561, 1561, 1562,\
				1562, 1562, 1562, 1562, 1563, 1563, 1563, 1563, 1563, 1564, 1564, 1564, 1565, 1565, 1565, 1566, 1566, 1566, 1567, 1567, 1567,\
				1568, 1568, 1568, 1569, 1569, 1569, 1570, 1570, 1570, 1571, 1571, 1571, 1572, 1572, 1572, 1573, 1573, 1573, 1574, 1574, 1574,\
				1575, 1575, 1575, 1576, 1576, 1576, 1577, 1577, 1577, 1578, 1578, 1578, 1579, 1579, 1579, 1580, 1580, 1580, 1581, 1581, 1581,\
				1582, 1583, 1584, 1584, 1585, 1585, 1585, 1585, 1586, 1586, 1586, 1586, 1587, 1587, 1587, 1587, 1587, 1588, 1588, 1588, 1588,\
				1589, 1590, 1591, 1592, 1593, 1594, 1595, 1596, 1597, 1598, 1599, 1600, 1601, 1602, 1603, 1604, 1605, 1606, 1607, 1608, 1609,\
				1610, 1611, 1612, 1613, 1614, 1615, 1616, 1617, 1618, 1619, 1620, 1621, 1622, 1623, 1624, 1625, 1626, 1627, 1628, 1629, 1630,\
				1631, 1632, 1633, 1634, 1635, 1636, 1637, 1638, 1639, 1640, 1641, 1642, 1643, 1644, 1645, 1646, 1647, 1648, 1649, 1650, 1651,\
				1652, 1653, 1654, 1655, 1656, 1657, 1658, 1659, 1660, 1661, 1662, 1663, 1664, 1665, 1666, 1667, 1668, 1669, 1670, 1671, 1672,\
				1673, 1674, 1675, 1676, 1677, 1678, 1679, 1680, 1681, 1682, 1683, 1684, 1685, 1686, 1687, 1688, 1689, 1690, 1691, 1692, 1693,\
				1694, 1695, 1696, 1697, 1698, 1699, 1700, 1701, 1702, 1703, 1704, 1705, 1706, 1707, 1708, 1709, 1710, 1711, 1712, 1713, 1714,\
				1715, 1716, 1717, 1718, 1719, 1720, 1721, 1722, 1723, 1724, 1725, 1726, 1727, 1728, 1729, 1730, 1731, 1732, 1733, 1734, 1735,\
				1736, 1737, 1738, 1739, 1740, 1741, 1742, 1743, 1744, 1745, 1746, 1747, 1748, 1749, 1750, 1751, 1751, 1752, 1752, 1753, 1753,\
				1754, 1754, 1754, 1755, 1755, 1755, 1756, 1756, 1756, 1756, 1756, 1757, 1757, 1757, 1757, 1758, 1758, 1758, 1758, 1759, 1759,\
				1759, 1760, 1760, 1760, 1761, 1761, 1761, 1762, 1762, 1763, 1763, 1763, 1764, 1764, 1764, 1765, 1765, 1766, 1766, 1766, 1767,\
				1767, 1767, 1768, 1768, 1768, 1769, 1769, 1769, 1770, 1770, 1771, 1771, 1771, 1772, 1772, 1772, 1773, 1773, 1773, 1774, 1774,\
				1774, 1775, 1775, 1776, 1776, 1776, 1777, 1777, 1777, 1778, 1778, 1778, 1779, 1779, 1779, 1780, 1780, 1780, 1781, 1781, 1781,\
				1782, 1782, 1782, 1783, 1783, 1784, 1784, 1784, 1785, 1785, 1785, 1786, 1786, 1786, 1787, 1787, 1787, 1788, 1788, 1788, 1789,\
				1789, 1789, 1790, 1790, 1790, 1791, 1791, 1791, 1792, 1792, 1792, 1793, 1793, 1793, 1794, 1794, 1794, 1795, 1795, 1795, 1796,\
				1796, 1796, 1797, 1797, 1797, 1798, 1798, 1798, 1799, 1799, 1799, 1800, 1800, 1800, 1800, 1801, 1801, 1801, 1802, 1802, 1802,\
				1803, 1803, 1803, 1804, 1804, 1804, 1805, 1805, 1805, 1806, 1806, 1806, 1807, 1807, 1807, 1808, 1808, 1808, 1808, 1809, 1809,\
				1809, 1810, 1810, 1810, 1811, 1811, 1811, 1812, 1812, 1812, 1813, 1813, 1813, 1813, 1814, 1814, 1814, 1815, 1815, 1815, 1816,\
				1816, 1816, 1816, 1817, 1817, 1817, 1818, 1818, 1818, 1819, 1819, 1819, 1820, 1820, 1820, 1820, 1821, 1821, 1821, 1822, 1822,\
				1822, 1823, 1823, 1823, 1823, 1824, 1824, 1824, 1825, 1825, 1825, 1826, 1826, 1826, 1826, 1827, 1827, 1827, 1828, 1828, 1828,\
				1828, 1829, 1829, 1829, 1830, 1830, 1830, 1831, 1831, 1831, 1831, 1832, 1832, 1832, 1833, 1833, 1833, 1833, 1834, 1834, 1834,\
				1835, 1835, 1835, 1835, 1836, 1836, 1836, 1837, 1837, 1837, 1837, 1838, 1838, 1838, 1839, 1839, 1839, 1839, 1840, 1840, 1840,\
				1841, 1841, 1841, 1841, 1842, 1842, 1842, 1842, 1843, 1843, 1843, 1844, 1844, 1844, 1844, 1845, 1845, 1845, 1846, 1846, 1846,\
				1846, 1847, 1847, 1847, 1847, 1848, 1848, 1848, 1849, 1849, 1849, 1849, 1850, 1850, 1850, 1850, 1851, 1851, 1851, 1852, 1852,\
				1852, 1852, 1853, 1853, 1853, 1853, 1854, 1854, 1854, 1854, 1855, 1855, 1855, 1856, 1856, 1856, 1856, 1857, 1857, 1857, 1857,\
				1858, 1858, 1858, 1858, 1859, 1859, 1859, 1860, 1860, 1860, 1860, 1861, 1861, 1861, 1861, 1862, 1862, 1862, 1862, 1863, 1863,\
				1863, 1863, 1864, 1864, 1864, 1864, 1865, 1865, 1865, 1865, 1866, 1866, 1866, 1867, 1867, 1867, 1867, 1868, 1868, 1868, 1868,\
				1869, 1869, 1869, 1869, 1870, 1870, 1870, 1870, 1871, 1871, 1871, 1871, 1872, 1872, 1872, 1872, 1873, 1873, 1873, 1873, 1874,\
				1874, 1874, 1874, 1875, 1875, 1875, 1875, 1876, 1876, 1876, 1876, 1877, 1877, 1877, 1877, 1878, 1878, 1878, 1878, 1879, 1879,\
				1879, 1879, 1880, 1880, 1880, 1880, 1881, 1881, 1881, 1881, 1882, 1882, 1882, 1882, 1882, 1883, 1883, 1883, 1883, 1884, 1884,\
				1884, 1884, 1885, 1885, 1885, 1885, 1886, 1886, 1886, 1886, 1887, 1887, 1887, 1887, 1888, 1888, 1888, 1888, 1888, 1889, 1889,\
				1889, 1889, 1890, 1890, 1890, 1890, 1891, 1891, 1891, 1891, 1892, 1892, 1892, 1892, 1892, 1893, 1893, 1893, 1893, 1894, 1894,\
				1894, 1894, 1895, 1895, 1895, 1895, 1896, 1896, 1896, 1896, 1896, 1897, 1897, 1897, 1897, 1898, 1898, 1898, 1898, 1899, 1899,\
				1899, 1899, 1899, 1900, 1900, 1900, 1900, 1901, 1901, 1901, 1901, 1901, 1902, 1902, 1902, 1902, 1903, 1903, 1903, 1903, 1904,\
				1904, 1904, 1904, 1904, 1905, 1905, 1905, 1905, 1906, 1906, 1906, 1906, 1906, 1907, 1907, 1907, 1907, 1908, 1908, 1908, 1908,\
				1908, 1909, 1909, 1909, 1909, 1910, 1910, 1910, 1910, 1910, 1911, 1911, 1911, 1911, 1912, 1912, 1912, 1912, 1912, 1913, 1913,\
				1913, 1913, 1913, 1914, 1914, 1914, 1914, 1915, 1915, 1915, 1915, 1915, 1916, 1916, 1916, 1916, 1917, 1917, 1917, 1917, 1917,\
				1918, 1918, 1918, 1918, 1918, 1919, 1919, 1919, 1919, 1920, 1920, 1920, 1920, 1920, 1921, 1921, 1921, 1921, 1921, 1922, 1922,\
				1922, 1922, 1922, 1923, 1923, 1923, 1923, 1924, 1924, 1924, 1924, 1924, 1925, 1925, 1925, 1925, 1925, 1926, 1926, 1926, 1926,\
				1926, 1927, 1927, 1927, 1927, 1927, 1928, 1928, 1928, 1928, 1929, 1929, 1929, 1929, 1929, 1930, 1930, 1930, 1930, 1930, 1931,\
				1931, 1931, 1931, 1931, 1932, 1932, 1932, 1932, 1932, 1933, 1933, 1933, 1933, 1933, 1934, 1934, 1934, 1934, 1934, 1935, 1935,\
				1935, 1935, 1935, 1936, 1936, 1936, 1936, 1936, 1937, 1937, 1937, 1937, 1937, 1938, 1938, 1938, 1938, 1938, 1939, 1939, 1939,\
				1939, 1939, 1940, 1940, 1940, 1940, 1940, 1941, 1941, 1941, 1941, 1941, 1942, 1942, 1942, 1942, 1942, 1943, 1943, 1943, 1943,\
				1943, 1944, 1944, 1944, 1944, 1944, 1945, 1945, 1945, 1945, 1945, 1946, 1946, 1946, 1946, 1946, 1947, 1947, 1947, 1947, 1947,\
				1947, 1948, 1948, 1948, 1948, 1948, 1949, 1949, 1949, 1949, 1949, 1950, 1950, 1950, 1950, 1950, 1951, 1951, 1951, 1951, 1951,\
				1952, 1952, 1952, 1952, 1952, 1952, 1953, 1953, 1953, 1953, 1953, 1954, 1954, 1954, 1954, 1954, 1955, 1955, 1955, 1955, 1955,\
				1956, 1956, 1956, 1956, 1956, 1956, 1957, 1957, 1957, 1957, 1957, 1958, 1958, 1958, 1958, 1958, 1958, 1959, 1959, 1959, 1959,\
				1959, 1960, 1960, 1960, 1960, 1960, 1961, 1961, 1961, 1961, 1961, 1961, 1962, 1962, 1962, 1962, 1962, 1963, 1963, 1963, 1963,\
				1963, 1963, 1964, 1964, 1964, 1964, 1964, 1965, 1965, 1965, 1965, 1965, 1965, 1966, 1966, 1966, 1966, 1966, 1967, 1967, 1967,\
				1967, 1967, 1967, 1968, 1968, 1968, 1968, 1968, 1969, 1969, 1969, 1969, 1969, 1969, 1970, 1970, 1970, 1970, 1970, 1971, 1971,\
				1971, 1971, 1971, 1971, 1972, 1972, 1972, 1972, 1972, 1972, 1973, 1973, 1973, 1973, 1973, 1974, 1974, 1974, 1974, 1974, 1974,\
				1975, 1975, 1975, 1975, 1975, 1975, 1976, 1976, 1976, 1976, 1976, 1977, 1977, 1977, 1977, 1977, 1977, 1978, 1978, 1978, 1978,\
				1978, 1978, 1979, 1979, 1979, 1979, 1979, 1979, 1980, 1980, 1980, 1980, 1980, 1980, 1981, 1981, 1981, 1981, 1981, 1982, 1982,\
				1982, 1982, 1982, 1982, 1983, 1983, 1983, 1983, 1983, 1983, 1984, 1984, 1984, 1984, 1984, 1984, 1985, 1985, 1985, 1985, 1985,\
				1985, 1986, 1986, 1986, 1986, 1986, 1986, 1987, 1987, 1987, 1987, 1987, 1987, 1988, 1988, 1988, 1988, 1988, 1988, 1989, 1989,\
				1989, 1989, 1989, 1989, 1990, 1990, 1990, 1990, 1990, 1990, 1991, 1991, 1991, 1991, 1991, 1991, 1992, 1992, 1992, 1992, 1992,\
				1992, 1993, 1993, 1993, 1993, 1993, 1993, 1994, 1994, 1994, 1994, 1994, 1994, 1995, 1995, 1995, 1995, 1995, 1995, 1996, 1996,\
				1996, 1996, 1996, 1996, 1997, 1997, 1997, 1997, 1997, 1997, 1998, 1998, 1998, 1998, 1998, 1998, 1999, 1999, 1999, 1999, 1999,\
				1999, 1999, 2000, 2000, 2000, 2000, 2000, 2000, 2001, 2001, 2001, 2001, 2001, 2001, 2002, 2002, 2002, 2002, 2002, 2002, 2003,\
				2003, 2003, 2003, 2003, 2003, 2003, 2004, 2004, 2004, 2004, 2004, 2004, 2005, 2005, 2005, 2005, 2005, 2005, 2006, 2006, 2006,\
				2006, 2006, 2006, 2006, 2007, 2007, 2007, 2007, 2007, 2007, 2008, 2008, 2008, 2008, 2008, 2008, 2009, 2009, 2009, 2009, 2009,\
				2009, 2009, 2010, 2010, 2010, 2010, 2010, 2010, 2011, 2011, 2011, 2011, 2011, 2011, 2011, 2012, 2012, 2012, 2012, 2012, 2012,\
				2013, 2013, 2013, 2013, 2013, 2013, 2013, 2014, 2014, 2014, 2014, 2014, 2014, 2015, 2015, 2015, 2015, 2015, 2015, 2015, 2016,\
				2016, 2016, 2016, 2016, 2016, 2017, 2017, 2017, 2017, 2017, 2017, 2017, 2018, 2018, 2018, 2018, 2018, 2018, 2019, 2019, 2019,\
				2019, 2019, 2019, 2019, 2020, 2020, 2020, 2020, 2020, 2020, 2020, 2021, 2021, 2021, 2021, 2021, 2021, 2022, 2022, 2022, 2022,\
				2022, 2022, 2022, 2023, 2023, 2023, 2023, 2023, 2023, 2023, 2024, 2024, 2024, 2024, 2024, 2024, 2024, 2025, 2025, 2025, 2025,\
				2025, 2025, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2027, 2027, 2027, 2027, 2027, 2027, 2027, 2028, 2028, 2028, 2028, 2028,\
				2028, 2028, 2029, 2029, 2029, 2029, 2029, 2029, 2029, 2030, 2030, 2030, 2030, 2030, 2030, 2030, 2031, 2031, 2031, 2031, 2031,\
				2031, 2031, 2032, 2032, 2032, 2032, 2032, 2032, 2032, 2033, 2033, 2033, 2033, 2033, 2033, 2033, 2034, 2034, 2034, 2034, 2034,\
				2034, 2034, 2035, 2035, 2035, 2035, 2035, 2035, 2035, 2036, 2036, 2036, 2036, 2036, 2036, 2036, 2037, 2037, 2037, 2037, 2037,\
				2037, 2037, 2038, 2038, 2038, 2038, 2038, 2038, 2038, 2039, 2039, 2039, 2039, 2039, 2039, 2039, 2040, 2040, 2047, 2047};

int ktd3137_brightness_table_reg4[256] = {0x01, 0x02, 0x04, 0x04, 0x07,
	0x02, 0x00, 0x06, 0x04, 0x02, 0x03, 0x04, 0x05, 0x06, 0x02,
	0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x04, 0x05,
	0x06, 0x05, 0x03, 0x00, 0x05, 0x02, 0x06, 0x02, 0x06, 0x02,
	0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02, 0x06, 0x02,
	0x06, 0x01, 0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x06, 0x01,
	0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x05, 0x07, 0x01, 0x03,
	0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02,
	0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x07,
	0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x07, 0x05, 0x03, 0x01,
	0x07, 0x05, 0x03, 0x01, 0x07, 0x05, 0x03, 0x01, 0x07, 0x05,
	0x03, 0x00, 0x05, 0x02, 0x07, 0x04, 0x01, 0x06, 0x03, 0x00,
	0x05, 0x02, 0x07, 0x04, 0x01, 0x06, 0x03, 0x00, 0x05, 0x02,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03,
	0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x07, 0x03, 0x06, 0x01,
	0x04, 0x07, 0x02, 0x05, 0x00, 0x03, 0x06, 0x01, 0x04, 0x07,
	0x02, 0x05, 0x00, 0x03, 0x06, 0x01, 0x04, 0x07, 0x02, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01,
	0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01,
	0x03, 0x05, 0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x05,
	0x07, 0x01, 0x03, 0x05, 0x07, 0x01, 0x03, 0x04, 0x05, 0x06,
	0x07};
int ktd3137_brightness_table_reg5[256] = {0x00, 0x06, 0x0C, 0x11, 0x15,
	0x1A, 0x1E, 0x21, 0x25, 0x29, 0x2C, 0x2F, 0x32, 0x35, 0x38, 0x3A,
	0x3D, 0x3F, 0x42, 0x44, 0x47, 0x49, 0x4C, 0x4E, 0x50, 0x52, 0x54,
	0x56, 0x58, 0x59, 0x5B, 0x5C, 0x5E, 0x5F, 0x61, 0x62, 0x64, 0x65,
	0x67, 0x68, 0x6A, 0x6B, 0x6D, 0x6E, 0x70, 0x71, 0x73, 0x74, 0x75,
	0x77, 0x78, 0x7A, 0x7B, 0x7C, 0x7E, 0x7F, 0x80, 0x82, 0x83, 0x85,
	0x86, 0x87, 0x88, 0x8A, 0x8B, 0x8C, 0x8D, 0x8F, 0x90, 0x91, 0x92,
	0x94, 0x95, 0x96, 0x97, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xAA, 0xAB, 0xAC,
	0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
	0xB8, 0xB9, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC0,
	0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC6, 0xC7, 0xC8, 0xC9, 0xC9,
	0xCA, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCF, 0xCF, 0xD0, 0xD1, 0xD2,
	0xD2, 0xD3, 0xD3, 0xD4, 0xD5, 0xD5, 0xD6, 0xD7, 0xD7, 0xD8, 0xD8,
	0xD9, 0xDA, 0xDA, 0xDB, 0xDC, 0xDC, 0xDD, 0xDD, 0xDE, 0xDE, 0xDF,
	0xDF, 0xE0, 0xE0, 0xE1, 0xE1, 0xE2, 0xE2, 0xE3, 0xE3, 0xE4, 0xE4, 0xE5,
	0xE5, 0xE6, 0xE6, 0xE7, 0xE7, 0xE8, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA, 0xEB,
	0xEB, 0xEC, 0xEC, 0xEC, 0xED, 0xED, 0xEE, 0xEE, 0xEE, 0xEF, 0xEF, 0xEF,
	0xF0, 0xF0, 0xF1, 0xF1, 0xF1, 0xF2, 0xF2, 0xF2, 0xF3, 0xF3, 0xF3, 0xF4,
	0xF4, 0xF4, 0xF4, 0xF5, 0xF5, 0xF5, 0xF5, 0xF6, 0xF6, 0xF6, 0xF6, 0xF7,
	0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF8, 0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xFA,
	0xFA, 0xFA, 0xFA, 0xFB, 0xFB, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD,
	0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF};

static int ktd3137_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int ktd3137_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	*val = ret;

	//LOG_DBG("Reading 0x%02x=0x%02x\n", reg, *val);
	return ret;
}

static int ktd3137_masked_write(struct i2c_client *client,
					int reg, u8 mask, u8 val)
{
	int rc;
	u8 temp = 0;

	rc = ktd3137_read_reg(client, reg, &temp);
	if (rc < 0) {
		dev_err(&client->dev, "failed to read reg\n");
	} else {
		temp &= ~mask;
		temp |= val & mask;
		rc = ktd3137_write_reg(client, reg, temp);
		if (rc < 0)
			dev_err(&client->dev, "failed to write masked data\n");
	}

	ktd3137_read_reg(client, reg, &temp);
	return rc;
}

static int ktd_find_bit(int x)
{
	int i = 0;

	while ((x = x >> 1))
		i++;

	return i+1;
}

static void ktd_parse_dt(struct device *dev, struct ktd3137_chip *chip)
{
	struct device_node *np = dev->of_node;
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	int rc = 0;
	u32 bl_channel, temp;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return;// -ENOMEM;

	pdata->hwen_gpio = of_get_named_gpio(np, "ktd,hwen-gpio", 0);
	LOG_DBG("hwen --<%d>\n", pdata->hwen_gpio);

	pdata->pwm_mode = of_property_read_bool(np, "ktd,pwm-mode");
	LOG_DBG("pwmmode --<%d>\n", pdata->pwm_mode);

	pdata->using_lsb = of_property_read_bool(np, "ktd,using-lsb");
	LOG_DBG("using_lsb --<%d>\n", pdata->using_lsb);

	if (pdata->using_lsb) {
		pdata->default_brightness = 0x7ff;
		pdata->max_brightness = 2047;
	} else {
		pdata->default_brightness = 0xff;
		pdata->max_brightness = 255;
	}
	rc = of_property_read_u32(np, "ktd,pwm-frequency", &temp);
	if (rc) {
		pr_err("Invalid pwm-frequency!\n");
	} else {
		pdata->pwm_period = temp;
		LOG_DBG("pwm-frequency --<%d>\n", pdata->pwm_period);
	}

	rc = of_property_read_u32(np, "ktd,bl-fscal-led", &temp);
	if (rc) {
		pr_err("Invalid backlight full-scale led current!\n");
	} else {
		pdata->full_scale_led = temp;
		LOG_DBG("full-scale led current --<%d mA>\n",
					pdata->full_scale_led);
	}

	rc = of_property_read_u32(np, "ktd,turn-on-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,,turnon!\n");
	} else {
		pdata->ramp_on_time = temp;
		LOG_DBG("ramp on time --<%d ms>\n", pdata->ramp_on_time);
	}

	rc = of_property_read_u32(np, "ktd,turn-off-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,,turnoff!\n");
	} else {
		pdata->ramp_off_time = temp;
		LOG_DBG("ramp off time --<%d ms>\n", pdata->ramp_off_time);
	}

	rc = of_property_read_u32(np, "ktd,pwm-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid pwm-tarns-dim value!\n");
	} else {
		pdata->pwm_trans_dim = temp;
		LOG_DBG("pwm trnasition dimming  --<%d ms>\n",
					pdata->pwm_trans_dim);
	}

	rc = of_property_read_u32(np, "ktd,i2c-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid i2c-trans-dim value !\n");
	} else {
		pdata->i2c_trans_dim = temp;
		LOG_DBG("i2c transition dimming --<%d ms>\n",
					pdata->i2c_trans_dim);
	}

	rc = of_property_read_u32(np, "ktd,bl-channel", &bl_channel);
	if (rc) {
		pr_err("Invalid channel setup\n");
	} else {
		pdata->channel = bl_channel;
		LOG_DBG("bl-channel --<%x>\n", pdata->channel);
	}

	rc = of_property_read_u32(np, "ktd,ovp-level", &temp);
	if (!rc) {
		pdata->ovp_level = temp;
		LOG_DBG("ovp-level --<%d> --temp <%d>\n",
					pdata->ovp_level, temp);
	} else
		pr_err("Invalid OVP level!\n");

	rc = of_property_read_u32(np, "ktd,switching-frequency", &temp);
	if (!rc) {
		pdata->frequency = temp;
		LOG_DBG("switching frequency --<%d>\n", pdata->frequency);
	} else {
		pr_err("Invalid Frequency value!\n");
	}

	rc = of_property_read_u32(np, "ktd,inductor-current", &temp);
	if (!rc) {
		pdata->induct_current = temp;
		LOG_DBG("inductor current limit --<%d>\n",
					pdata->induct_current);
	} else
		pr_err("invalid induct_current limit\n");

	rc = of_property_read_u32(np, "ktd,flash-timeout", &temp);
	if (!rc) {
		pdata->flash_timeout = temp;
		LOG_DBG("flash timeout --<%d>\n", pdata->flash_timeout);
	} else {
		pr_err("invalid flash-time value!\n");
	}

	rc = of_property_read_u32(np, "ktd,linear_ramp", &temp);
	if (!rc) {
		pdata->linear_ramp = temp;
		LOG_DBG("linear_ramp --<%d>\n", pdata->linear_ramp);
	} else {
		pr_err("invalid linear_ramp value!\n");
	}

	rc = of_property_read_u32(np, "ktd,linear_backlight", &temp);
	if (!rc) {
		pdata->linear_backlight = temp;
		LOG_DBG("linear_backlight --<%d>\n", pdata->linear_backlight);
	} else {
		pr_err("invalid linear_backlight value!\n");
	}

	rc = of_property_read_u32(np, "ktd,flash-current", &temp);
	if (!rc) {
		pdata->flash_current = temp;
		LOG_DBG("flash current --<0x%x>\n", pdata->flash_current);
	} else {
		pr_err("invalid flash current value!\n");
	}

	dev->platform_data = pdata;
}

static int ktd3137_bl_enable_channel(struct ktd3137_chip *chip)
{
	int ret;
	struct ktd3137_bl_pdata *pdata = chip->pdata;

	if (pdata->channel == 0) {
		//default value for mode Register, all channel disabled.
		LOG_DBG("all channels are going to be disabled\n");
		ret = ktd3137_write_reg(chip->client, REG_PWM, 0x18);
	} else if (pdata->channel == 3) {
		LOG_DBG("turn all channel on!\n");
		ret = ktd3137_masked_write(chip->client, REG_PWM, 0x9F, 0x9F);
	} else if (pdata->channel == 2) {
		ret = ktd3137_masked_write(chip->client, REG_PWM, 0x9F, 0x1B);
	}

	return ret;
}

static void ktd3137_pwm_mode_enable(struct ktd3137_chip *chip, bool en)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	u8 value;

	if (en) {
		if (pdata->pwm_mode)
			LOG_DBG("already activated!\n");
		else
			pdata->pwm_mode = en;
		ktd3137_masked_write(chip->client, REG_PWM, 0x80, 0x80);
	} else {
		if (pdata->pwm_mode)
			pdata->pwm_mode = en;
		ktd3137_masked_write(chip->client, REG_PWM, 0x9F, 0x1B);
	}

	ktd3137_read_reg(chip->client, REG_PWM, &value);
	LOG_DBG("register pwm<0x06> current value is --<%x>\n", value);
}

static void ktd3137_get_deviceid(struct ktd3137_chip *chip)
{
	u8 value;

	ktd3137_read_reg(chip->client, REG_DEV_ID, &value);
	LOG_DBG("Device ID is --<0x0%x>\n", (value >> 3));
}

static void ktd3137_ramp_setting(struct ktd3137_chip *chip)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	unsigned int max_time = 16384;
	int temp = 0;

	if (pdata->ramp_on_time == 0) {//512us
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0xf0, 0x00);
		LOG_DBG("rampon time is 0\n");
	} else if (pdata->ramp_on_time > max_time) {
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0xf0, 0xf0);
		LOG_DBG("rampon time is max\n");
	} else {
		temp = ktd_find_bit(pdata->ramp_on_time);
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0xf0, temp<<4);
		LOG_DBG("temp is %d\n", temp);
	}

	if (pdata->ramp_off_time == 0) {//512us
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0x0f, 0x00);
		LOG_DBG("rampoff time is 0\n");
	} else if (pdata->ramp_off_time > max_time) {
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0x0f, 0x0f);
		LOG_DBG("rampoff time is max\n");
	} else {
		temp = ktd_find_bit(pdata->ramp_off_time);
		ktd3137_masked_write(chip->client, REG_RAMP_ON, 0x0f, temp);
		LOG_DBG("temp is %d\n", temp);
	}

}

static void ktd3137_transition_ramp(struct ktd3137_chip *chip)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	int reg_i2c, reg_pwm, temp;

	if (pdata->i2c_trans_dim >= 1024) {
		reg_i2c = 0xf;
	} else if (pdata->i2c_trans_dim < 128) {
		reg_i2c = 0x0;
	} else {
		temp = pdata->i2c_trans_dim/64;
		reg_i2c = temp-1;
		LOG_DBG("reg_i2c is --<0x%x>\n", reg_i2c);
	}

	if (pdata->pwm_trans_dim >= 256) {
		reg_pwm = 0x7;
	} else if (pdata->pwm_trans_dim < 4) {
		reg_pwm = 0x0;
	} else {
		temp = ktd_find_bit(pdata->pwm_trans_dim);
		reg_pwm = temp - 2;
		LOG_DBG("temp is %d\n", temp);
	}

	ktd3137_masked_write(chip->client, REG_TRANS_RAMP, 0x70, reg_pwm);
	ktd3137_masked_write(chip->client, REG_TRANS_RAMP, 0x0f, reg_i2c);

}

static void ktd3137_flash_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct ktd3137_chip *chip;
	u8 reg;

	chip = container_of(cdev, struct ktd3137_chip, cdev_flash);

	cancel_delayed_work_sync(&chip->work);
	if (!brightness) // flash off
		return;
	else if (brightness > 15)
		brightness = 0x0f;

	if (chip->pdata->flash_timeout < 100)
		reg = 0x00;
	else if (chip->pdata->flash_timeout > 1500)
		reg = 0x0f;
	else
		reg = (chip->pdata->flash_timeout/100);

	reg = (reg << 4) | brightness;
	LOG_DBG("update register value --<0x%x>\n", reg);
	ktd3137_write_reg(chip->client, REG_FLASH_SETTING, reg);

	ktd3137_masked_write(chip->client, REG_MODE, 0x02, 0x02);

	schedule_delayed_work(&chip->work, chip->pdata->flash_timeout);
}

static int ktd3137_flashled_init(struct i2c_client *client,
				struct ktd3137_chip *chip)
{
	//struct ktd3137_bl_pdata *pdata = chip->pdata;
	int ret;

	chip->cdev_flash.name = "ktd3137_flash";
	chip->cdev_flash.max_brightness = 16;
	chip->cdev_flash.brightness_set = ktd3137_flash_brightness_set;

	ret = led_classdev_register((struct device *) &client->dev,
						&chip->cdev_flash);
	if (ret < 0) {
		dev_err(&client->dev, "failed to register ktd3137_flash\n");
		return ret;
	}

	return 0;
}

static void ktd3137_backlight_init(struct ktd3137_chip *chip)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	u8 value;
	u8 update_value;
	ktd3137_get_deviceid(chip);
	update_value = (pdata->ovp_level == 32) ? 0x20 : 0x00;
	(pdata->induct_current == 2600) ? update_value |= 0x08 : update_value;
	(pdata->frequency == 1000) ? update_value |= 0x40 : update_value;
	(pdata->linear_ramp == 1) ? update_value |= 0x04 : update_value;
	(pdata->linear_backlight == 1) ? update_value |= 0x02 : update_value;
	ktd3137_write_reg(chip->client, REG_CONTROL, update_value);
	ktd3137_bl_enable_channel(chip);

	if (pdata->pwm_mode) {
		ktd3137_pwm_mode_enable(chip, true);
	} else {
		ktd3137_pwm_mode_enable(chip, false);
	}

	ktd3137_ramp_setting(chip);
	ktd3137_transition_ramp(chip);
	ktd3137_read_reg(chip->client, REG_CONTROL, &value);
	ktd3137_masked_write(chip->client, REG_MODE, 0xf8,
					pdata->full_scale_led);

	LOG_DBG("read control register -before--<0x%x> -after--<0x%x>\n",
					update_value, value);
}

static void ktd3137_hwen_pin_ctrl(struct ktd3137_chip *chip, int en)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;

	if (en) {
		LOG_DBG("hwen pin is going to be high!---<%d>\n", en);
		gpio_set_value(pdata->hwen_gpio, true);
	} else {
		LOG_DBG("hwen pin is going to be low!---<%d>\n", en);
		gpio_set_value(pdata->hwen_gpio, false);
	}
}

static void ktd3137_check_status(struct ktd3137_chip *chip)
{
	u8 value = 0;

	ktd3137_read_reg(chip->client, REG_STATUS, &value);
	if (value) {
		LOG_DBG("status bit has been change! <%x>", value);

		if (value & RESET_CONDITION_BITS) {
			ktd3137_hwen_pin_ctrl(chip, 0);
			ktd3137_hwen_pin_ctrl(chip, 1);
			ktd3137_backlight_init(chip);
		}

	}
	return ;//value;
}

static int ktd3137_gpio_init(struct ktd3137_chip *chip)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	int ret;

	if (gpio_is_valid(pdata->hwen_gpio)) {
		ret = gpio_request(pdata->hwen_gpio, "ktd_hwen_gpio");
		if (ret < 0) {
			pr_err("failed to request gpio\n");
			return  -ENOMEM;
		}
		ret = gpio_direction_output(pdata->hwen_gpio, 0);
		if (ret < 0) {
			pr_err("failed to set output");
			gpio_free(pdata->hwen_gpio);
			return ret;
		}
		LOG_DBG("gpio is valid!\n");
		ktd3137_hwen_pin_ctrl(chip, 1);
	}

	return 0;
}

static void ktd3137_pwm_control(struct ktd3137_chip *chip, int brightness)
{
	struct pwm_device *pwm = NULL;
	unsigned int duty, period;

	if (!chip->pwm) {
		pwm = devm_pwm_get(chip->dev, DEFAULT_PWM_NAME);

		if (IS_ERR(pwm)) {
			dev_err(chip->dev, "can't get pwm device\n");
			return;
		}
	}

	if (brightness > chip->pdata->max_brightness)
		brightness = chip->pdata->max_brightness;

	chip->pwm = pwm;
	period = chip->pdata->pwm_period;
	duty = brightness * period / chip->pdata->max_brightness;
	pwm_config(chip->pwm, duty, period);

	if (duty)
		pwm_enable(chip->pwm);
	else
		pwm_disable(chip->pwm);
}

void ktd3137_brightness_set_workfunc(struct ktd3137_chip *chip, int brightness)
{
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	u8 value;

	if (brightness == 0) {
		ktd3137_write_reg(chip->client, 0x07, 0x04);
		ktd3137_write_reg(chip->client, REG_MODE, 0x98);
		mdelay(10);
	} else {
		if (ktd_hbm_mode == 3) {
			ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xE1);//27.4mA
			LOG_DBG(KERN_INFO "[%s]: ktd_hbm_mode = %d\n", __func__, ktd_hbm_mode);
		} else if (ktd_hbm_mode == 2) {
			ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xC9);//25mA
			LOG_DBG(KERN_INFO "[%s]: ktd_hbm_mode = %d\n", __func__, ktd_hbm_mode);
		} else {
			ktd3137_write_reg(chip->client, REG_MODE, 0xA9);//21.8mA
			LOG_DBG(KERN_INFO "[%s]: ktd_hbm_mode = %d\n", __func__, ktd_hbm_mode);
		}
	}

	if (pdata->linear_backlight == 1) {
		ktd3137_masked_write(chip->client, REG_CONTROL, 0x02, 0x02);// set linear mode
	}

	if (pdata->pwm_mode) {
		LOG_DBG("pwm_ctrl is needed\n");
		ktd3137_pwm_control(chip, brightness);
	} else {
		if (brightness > pdata->max_brightness)
			brightness = pdata->max_brightness;
		if (pdata->using_lsb) {
			brightness = ktd_Exponential[brightness];
			ktd3137_masked_write(chip->client, REG_RATIO_LSB,
							0x07, brightness);
			ktd3137_masked_write(chip->client, REG_RATIO_MSB,
							0xff, brightness>>3);
		} else {
			ktd3137_masked_write(chip->client, REG_RATIO_LSB, 0x07,
				ktd3137_brightness_table_reg4[brightness]);
			ktd3137_masked_write(chip->client, REG_RATIO_MSB, 0xff,
				ktd3137_brightness_table_reg5[brightness]);
		}
	}

	ktd3137_read_reg(chip->client, 0x02, &value);
	ktd3137_read_reg(chip->client, 0x03, &value);
	ktd3137_read_reg(chip->client, 0x04, &value);
	ktd3137_read_reg(chip->client, 0x05, &value);
	ktd3137_read_reg(chip->client, 0x06, &value);
	ktd3137_read_reg(chip->client, 0x08, &value);
}

int ktd_hbm_set(enum backlight_hbm_mode hbm_mode)
{
	u8 value = 0;
	LOG_DBG("%s enter\n", __func__);

	ktd_hbm_mode = hbm_mode;
	ktd3137_read_reg(bkl_chip->client, 0x02, &value);
	LOG_DBG("default cur is %x", value);
	switch (hbm_mode) {
	case HBM_MODE_DEFAULT:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xA9);//21.8mA
		LOG_DBG("This is hbm mode 1\n");
		break;
	case HBM_MODE_LEVEL1:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xC9);//25mA
		LOG_DBG("This is hbm mode 2\n");
		break;
	case HBM_MODE_LEVEL2:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xE1);//27.4mA
		LOG_DBG("This is hbm mode 3\n");
		break;
	default:
		LOG_DBG("This isn't hbm mode\n");
		break;
	 }
	ktd3137_read_reg(bkl_chip->client, 0x02, &value);
	LOG_DBG("[bkl]%s hbm_mode=%d, value = %d\n", __func__, hbm_mode, value);
	return 0;
}

int ktd3137_brightness_set(int brightness)
{
	LOG_DBG("%s brightness = %d\n", __func__, brightness);

	ktd3137_brightness_set_workfunc(bkl_chip, brightness);
	return brightness;
}

static int ktd3137_update_brightness(struct backlight_device *bl)
{
	struct ktd3137_chip *chip = bl_get_data(bl);
	int brightness = bl->props.brightness;

	LOG_DBG("current brightness is --<%d>\n", bl->props.brightness);

	cancel_delayed_work_sync(&chip->work);

	if (bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		brightness = 0;

	if (brightness > 0)
		ktd3137_masked_write(chip->client, REG_MODE, 0x01, 0x01);
	else
		ktd3137_masked_write(chip->client, REG_MODE, 0x01, 0x00);

	ktd3137_brightness_set_workfunc(chip, brightness);
	schedule_delayed_work(&chip->work, 100);

	return 0;
}

static const struct backlight_ops ktd3137_backlight_ops = {
	.options    = BL_CORE_SUSPENDRESUME,
	.update_status = ktd3137_update_brightness,
};

static ssize_t ktd3137_bl_chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_DEV_ID, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_mode_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_MODE, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_mode_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_MODE, value);

	return count;
}

static ssize_t ktd3137_bl_ctrl_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_CONTROL, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_ctrl_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_CONTROL, value);

	return count;

}

static ssize_t ktd3137_bl_brightness_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint16_t reg_val = 0;
	u8 temp = 0;

	ktd3137_read_reg(chip->client, REG_RATIO_LSB, &temp);
	reg_val = temp << 8;
	ktd3137_read_reg(chip->client, REG_RATIO_MSB, &temp);
	reg_val |= temp;

	return snprintf(buf, 1024, "0x%x\n", reg_val);

}

static ssize_t ktd3137_bl_brightness_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}


	if (chip->pdata->using_lsb) {
		ktd3137_masked_write(chip->client, REG_RATIO_LSB,
							0x07, value);
		ktd3137_masked_write(chip->client, REG_RATIO_MSB,
							0xff, value >> 3);
	} else {
		ktd3137_write_reg(chip->client, REG_RATIO_MSB, value);
	}

	return count;
}

static ssize_t ktd3137_bl_pwm_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_PWM, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_pwm_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_PWM, value);

	return count;
}

static ssize_t ktd3137_bl_ramp_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_RAMP_ON, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_ramp_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_RAMP_ON, value);

	return count;

}

static ssize_t ktd3137_bl_trans_ramp_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_TRANS_RAMP, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_trans_ramp_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_TRANS_RAMP, value);

	return count;

}

static ssize_t ktd3137_bl_flash_setting_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_FLASH_SETTING, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static ssize_t ktd3137_bl_flash_setting_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(chip->dev, "%s: failed to store!\n", __func__);
		return ret;
	}

	ktd3137_write_reg(chip->client, REG_FLASH_SETTING, value);

	return count;
}

static ssize_t ktd3137_bl_status_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd3137_chip *chip = dev_get_drvdata(dev);
	uint8_t reg_val = 0;

	ktd3137_read_reg(chip->client, REG_STATUS, &reg_val);
	return snprintf(buf, 1024, "0x%x\n", reg_val);
}

static DEVICE_ATTR(ktd_chip_id, 0444, ktd3137_bl_chip_id_show, NULL);
static DEVICE_ATTR(ktd_mode_reg, 0664, ktd3137_bl_mode_reg_show,
				ktd3137_bl_mode_reg_store);
static DEVICE_ATTR(ktd_ctrl_reg, 0664, ktd3137_bl_ctrl_reg_show,
				ktd3137_bl_ctrl_reg_store);
static DEVICE_ATTR(ktd_brightness_reg, 0664, ktd3137_bl_brightness_reg_show,
				ktd3137_bl_brightness_reg_store);
static DEVICE_ATTR(ktd_pwm_reg, 0664, ktd3137_bl_pwm_reg_show,
				ktd3137_bl_pwm_reg_store);
static DEVICE_ATTR(ktd_ramp_reg, 0664, ktd3137_bl_ramp_reg_show,
				ktd3137_bl_ramp_reg_store);
static DEVICE_ATTR(ktd_trans_ramp_reg, 0664, ktd3137_bl_trans_ramp_reg_show,
				ktd3137_bl_trans_ramp_reg_store);
static DEVICE_ATTR(ktd_flash_setting_reg, 0664,
				ktd3137_bl_flash_setting_reg_show,
				ktd3137_bl_flash_setting_reg_store);
static DEVICE_ATTR(ktd_status_reg, 0444, ktd3137_bl_status_reg_show, NULL);

static struct attribute *ktd3137_bl_attribute[] = {
	&dev_attr_ktd_chip_id.attr,
	&dev_attr_ktd_mode_reg.attr,
	&dev_attr_ktd_ctrl_reg.attr,
	&dev_attr_ktd_brightness_reg.attr,
	&dev_attr_ktd_pwm_reg.attr,
	&dev_attr_ktd_ramp_reg.attr,
	&dev_attr_ktd_trans_ramp_reg.attr,
	&dev_attr_ktd_flash_setting_reg.attr,
	&dev_attr_ktd_status_reg.attr,
	NULL
};

static const struct attribute_group ktd3137_bl_attr_group = {
	.attrs = ktd3137_bl_attribute,
};

static void ktd3137_sync_backlight_work(struct work_struct *work)
{
	struct ktd3137_chip *chip;
	u8 value;

	chip = container_of(work, struct ktd3137_chip, work.work);

	ktd3137_read_reg(chip->client, REG_FLASH_SETTING, &value);
	//LOG_DBG("flash setting register --<0x%x>\n", value);

	ktd3137_read_reg(chip->client, REG_MODE, &value);
	//LOG_DBG("mode register --<0x%x>\n", value);

	ktd3137_check_status(chip);
}

/*static int ktd3137_backlight_add_device(struct i2c_client *client,
				struct ktd3137_chip *chip)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	struct ktd3137_bl_pdata *pdata = chip->pdata;
	int ret;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.brightness = pdata->default_brightness;
	props.max_brightness = pdata->max_brightness;

	bl = devm_backlight_device_register(&client->dev,
			dev_driver_string(&client->dev),
			&client->dev, chip, &ktd3137_backlight_ops, &props);

	if (IS_ERR(bl)) {
		dev_err(&client->dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}

	chip->bl = bl;

	ret = sysfs_create_group(&chip->bl->dev.kobj, &ktd3137_bl_attr_group);
	if (ret) {
		dev_err(&client->dev, "failed to create sysfs group\n");
		return ret;
	}

	return 0;
}*/

static struct class *ktd3137_class;
static atomic_t ktd_dev;
static struct device *ktd3137_dev;

struct device *ktd3137_device_create(void *drvdata, const char *fmt)
{
	struct device *dev;
	if (IS_ERR(ktd3137_class)) {
		pr_err("Failed to create class %ld\n", PTR_ERR(ktd3137_class));
	}

	dev = device_create(ktd3137_class, NULL, atomic_inc_return(&ktd_dev), drvdata, fmt);
	if (IS_ERR(dev)) {
		pr_err("Failed to create device %s %ld\n", fmt, PTR_ERR(dev));
	} else {
		pr_debug("%s : %s : %d\n", __func__, fmt, dev->devt);
	}

	return dev;
}


static int ktd3137_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	int err = 0;
	u8 value;

	struct ktd3137_bl_pdata *pdata = dev_get_drvdata(&client->dev);
	struct ktd3137_chip *chip;
	//struct device_node *np = client->dev.of_node;
	extern char *saved_command_line;
	int bkl_id = 0;
	char *bkl_ptr = (char *)strnstr(saved_command_line, ":bklic=", strlen(saved_command_line));
	bkl_ptr += strlen(":bklic=");
	bkl_id = simple_strtol(bkl_ptr, NULL, 10);
	if (bkl_id != 24) {
		return -ENODEV;
	}

	client->addr = 0x36;
	LOG_DBG("probe start!\n");
	if (!pdata) {
		ktd_parse_dt(&client->dev, chip);
		pdata = dev_get_platdata(&client->dev);
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "check_functionality failed.\n");
		err = -ENODEV;
		goto exit0;
	}

	//ktd3137_client = client;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto exit0;
	}

	chip->client = client;
	chip->pdata = pdata;
	chip->dev = &client->dev;

	ktd_hbm_mode = 0;

	ktd3137_dev = ktd3137_device_create(chip, "ktd");
	if (IS_ERR(ktd3137_dev)) {
		dev_err(&client->dev, "failed_to create device for ktd");
	}

	err = sysfs_create_group(&ktd3137_dev->kobj, &ktd3137_bl_attr_group);
	if (err) {
		dev_err(&client->dev, "failed to create sysfs group\n");

	}

	i2c_set_clientdata(client, chip);

	/*err = ktd3137_backlight_add_device(client, chip);
	if (err)
		goto exit0;*/

	ktd3137_gpio_init(chip);
	ktd3137_backlight_init(chip);
	ktd3137_write_reg(chip->client, 0x08, 0x01);
	INIT_DELAYED_WORK(&chip->work, ktd3137_sync_backlight_work);
	ktd3137_flashled_init(client, chip);
	ktd3137_check_status(chip);
	ktd3137_read_reg(chip->client, 0x02, &value);
	ktd3137_read_reg(chip->client, 0x03, &value);
	ktd3137_read_reg(chip->client, 0x06, &value);
	ktd3137_read_reg(chip->client, 0x07, &value);
	ktd3137_read_reg(chip->client, 0x08, &value);
	ktd3137_read_reg(chip->client, 0x0A, &value);
	//backlight_update_status(chip->bl);
	bkl_chip = chip;
exit0:
	return err;
}

static int ktd3137_remove(struct i2c_client *client)
{
	struct ktd3137_chip *chip = i2c_get_clientdata(client);

	chip->bl->props.brightness = 0;

	backlight_update_status(chip->bl);
	cancel_delayed_work_sync(&chip->work);

	ktd3137_hwen_pin_ctrl(chip, 0);

	sysfs_remove_group(&chip->bl->dev.kobj, &ktd3137_bl_attr_group);
	gpio_free(chip->pdata->hwen_gpio);

	return 0;
}

static const struct i2c_device_id ktd3137_id[] = {
	{KTD_I2C_NAME, 6},
	{ }
};


static const struct of_device_id ktd3137_match_table[] = {
	{ .compatible = "ktd,ktd3137",},
	{ },
};

static struct i2c_driver ktd3137_driver = {
	.driver = {
		.name	= KTD_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ktd3137_match_table,
	},
	.probe = ktd3137_probe,
	.remove = ktd3137_remove,
	.id_table = ktd3137_id,
};

static int __init ktd3137_init(void)
{
	int err;

	ktd3137_class = class_create(THIS_MODULE, "ktd3137");
	if (IS_ERR(ktd3137_class)) {
		pr_err("unable to create ktd3137 class; errno = %ld\n", PTR_ERR(ktd3137_class));
		ktd3137_class = NULL;
	}

	err = i2c_add_driver(&ktd3137_driver);
	if (err) {
		LOG_DBG("ktd3137 driver failed,(errno = %d)\n", err);
	} else {
		LOG_DBG("Successfully added driver %s\n",
			ktd3137_driver.driver.name);
	}
	return err;
}

static void __exit ktd3137_exit(void)
{
	i2c_del_driver(&ktd3137_driver);
}

module_init(ktd3137_init);
module_exit(ktd3137_exit);

MODULE_AUTHOR("kinet-ic.com");
MODULE_LICENSE("GPL");
